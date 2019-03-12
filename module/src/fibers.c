#include "fibers.h"
#include "common.h"

#include <linux/slab.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <asm/thread_info.h>
#include <linux/sched/task_stack.h>
#include <linux/types.h>
#include <linux/bitops.h>

#define FLS_SIZE 4096

// To keep track of free slots in FLS
struct fls_free_ll{
    
    long index;
    
    struct fls_free_ll * next;
};

// Mantains the cpu context associated with the workflow of this fiber.
struct fiber{

    atomic_t           active_pid;   // 0 if there is no active thread, ensure
                                     // mutual exclusion from SwitchToFiber 
    
    /* @TODO set active_pid atomically and avoid usage of this spinlock
    spinlock_t      fiber_lock;   // Needed to ensure that no two threads
                                  // try to run the same fiber.
    unsigned long   flags;        // Needed to correctly restore interrupts
    */ 

    struct pt_regs  pt_regs;      // These two fields will store the state
    struct fpu      fpu;          // of the CPU of this fiber

    void           *stack_base;   // Base of allocated stack, to be freed
    unsigned long   stack_size;   // Size of the allocated stack 


    // These attributes are needed to add struct fiber into an hashtable    
    pid_t             fid;   // key for hashtable
    struct hlist_node fnext; // Needed to be added into an hastable
    
        
    // FLS-related fields
    long long * fls;//[FLS_SIZE];
    // Bitmap to check for used slots
    unsigned long * fls_used_bmp;
    
    // LinkedList to keep slots inbetween
    struct fls_free_ll * free_ll;
    // Bitmap to check if a slot is pointed by a LL node
    unsigned long * fls_pointed_bmp;
    
    int used_fls;
    
};

// Mantains the responsibility of fibers for each process
struct process{

    DECLARE_HASHTABLE(fibers,6);  // Mantains the list of fibers that are
                                  // activated by a thread that belongs to
                                  // this process for cleanup purposes.
    
    DECLARE_HASHTABLE(threads,6); // Needed to check if a given thread has
                                  // already been converted to fiber.

    atomic_t last_fid;            // Needed to generate a new fiber id on
                                  // each subsequent CreateFiber().


    // These attributes are needed to add struct process into an hashtable
    pid_t tgid;               // key for hashtable
    struct hlist_node pnext;  // Needed to be added into an hastable
};

// Mantain thread activated fiber
struct thread{
    atomic_t active_fid;

    // These attributes are needed to add struct process into an hashtable
    pid_t pid;                // key for hashtable
    struct hlist_node tnext;  // Needed to be added into an hastable
};


DEFINE_HASHTABLE(processes,6);
DEFINE_SPINLOCK(processes_lock); // processes hashtable spinlock.(RW LOCK?)


// get_x_by_id are auxiliary functions
// If no matching entry is found, they return NULL
inline struct process * get_process_by_id(pid_t tgid){
    
    struct process *p;
    
    hash_for_each_possible_rcu(processes, p, pnext, tgid){
        if(p==NULL) break;
        if(p->tgid==tgid) break;
    }
    
    return p;
}

inline struct thread * get_thread_by_id(pid_t pid, struct process * p){
    
    struct thread *t;
    
    hash_for_each_possible_rcu(p->threads, t, tnext, pid){
        if(t==NULL) break;
        if(t->pid==pid) break;
    }
    
    return t;
}

inline struct fiber * get_fiber_by_id(pid_t fid, struct process * p){
    
    struct fiber *f;
    
    hash_for_each_possible_rcu(p->fibers, f, fnext, fid){
        if(f==NULL) break;
        if(f->fid==fid) break;
    }
    
    return f;
}


pid_t kernelConvertThreadToFiber(pid_t tgid,pid_t pid){
    struct process *p;
    struct thread  *t;
    struct fiber   *f;

    unsigned long flags;

    log("kernelConvertThreadToFiber tgid:%d, pid:%d\n",tgid,pid);
    
    
    // Access bucket with key tgid and search for the struct process
    spin_lock_irqsave(&processes_lock,flags);
    
    p = get_process_by_id(tgid);

    if(!p){    // First thread of process that is going to be 
                    //converted to Fiber.
        dbg("There was no process %d in the hashtable, lets create one.\n",tgid);

        p= kmalloc(sizeof(struct process),GFP_KERNEL);
        if(!p){
            log("ConvertThreadToFiber, error allocating struct process.\n");
            return ERROR;
        }

        p->tgid = tgid;
        atomic_set(&(p->last_fid),0);
        hash_init(p->fibers);
        hash_init(p->threads);
        hash_add_rcu(processes,&(p->pnext),p->tgid);

    }

    spin_unlock_irqrestore(&processes_lock,flags);

    // Create a new thread struct only if it hadn't been created yet    
    t = get_thread_by_id(pid, p);
    
    if(t){ // thread already was a fiber
        dbg("Error converting thread %d to fiber, it already exists in p->threads.\n",pid);
        return ERROR;
    }
    
    t= kmalloc(sizeof(struct thread),GFP_KERNEL);
    if(!t) {
        log("ConvertThreadToFiber, error allocating struct thread.\n");
        return ERROR;
    }


    t->pid=pid;
    //t->active_fid is set below
    hash_add_rcu(p->threads,&(t->tnext),t->pid);


    // Create a new fiber, activated by this thread.    
    f= kmalloc(sizeof(struct fiber), GFP_KERNEL);
    
    atomic_set(&(f->active_pid),pid);
    
    f->stack_base = NULL; // A Fiber created from an existing Thread
    f->stack_size = 0;    // has not a newly allocated stack
    
    f->fid = atomic_fetch_inc(&(p->last_fid));
    atomic_set(&(t->active_fid),f->fid);
    
    // FLS management
    f->used_fls = 0;
    
    dbg("A new fiber with fid %d is created, with active_pid %d\n",f->fid,atomic_read(&(f->active_pid)));

    hash_add_rcu(p->fibers,&(f->fnext),f->fid); 

    return f->fid;
}

pid_t kernelCreateFiber(long user_fn, void *param, pid_t tgid,pid_t pid, void *stack_base, size_t stack_size){


    struct process *p;
    struct thread  *t;
    struct fiber   *f;

    log("kernelCreateFiber\n");

    // Check if struct process with given tgid exists or
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error creating fiber, process %d still not created into processes hashtable",tgid);
        return ERROR;        
    }
    
    // Check if struct thread with given pid exists
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error creating fiber, thread %d still not created into %d->threads",pid,tgid);
        return ERROR;
    }

    // Create a new struct fiber with given function and stack
    // Initially registers are not set because they are needed to store 
    // data when a running fiber is scheduled out, only rip is set.

    f= kmalloc(sizeof(struct fiber),GFP_KERNEL);
    if(!f){
        log("CreateFiber, error allocating struct fiber");
        return ERROR;
    }

    f->fid = atomic_fetch_inc(&(p->last_fid)) ;
    atomic_set(&(f->active_pid),0);

    f->stack_base = stack_base;
    f->stack_size = stack_size;
    
    memcpy(&(f->pt_regs), task_pt_regs(current), sizeof(struct pt_regs));
    
    f->pt_regs.ip = (long) user_fn;
    //f->pt_regs.cx = (long) user_fn;
    f->pt_regs.di = (long) param;
    f->pt_regs.sp = (long) (stack_base + stack_size) - 8;
     
    f->pt_regs.bp = f->pt_regs.sp;
    
    // FLS management
    f->used_fls = 0;
    
   
    dbg("Inserting a new fiber fid %d with active_pid %d and RIP %ld",f->fid,atomic_read(&(f->active_pid)),(long)f->pt_regs.ip);
    
    hash_add_rcu(p->fibers,&(f->fnext),f->fid);
    
    return f->fid;
}

pid_t kernelSwitchToFiber(pid_t tgid, pid_t pid, pid_t fid){
    
    struct process *p;
    struct thread  *t;
    struct fiber   *dst_f;
    struct fiber   *src_f;
    struct pt_regs *cpu_regs;
    long src_fid,old;
    log("kernelSwitchToFiber tgid:%d pid:%d fid:%d\n",tgid,pid,fid);


    // Check if struct process exists otherwise return error
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error SwitchToFiber, process %d still not created.\n",tgid);
        return ERROR;   // In the current process no thread has
                        // been converted to fiber yet
    }
    
    // Check if current thread has been converted to fiber otherwise error
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error SwitchToFiber, thread %d still not in %d->threads\n",pid,tgid);
        return ERROR;    // Calling thread was not converted yet
    }
        
    src_fid=atomic_read(&(t->active_fid));
    
    // Find target fiber
    dst_f = get_fiber_by_id(fid, p);
    if (!dst_f){
        dbg("Error SwitchToFiber, fiber %d not created yet\n",fid);
        return ERROR;    // Target fiber does not exist
    }
    dbg("SwitchToFiber, found dest_fiber %d has active_pid %d\n",fid,atomic_read(&(dst_f->active_pid)));

    // Check if target fiber is already in use and book it for the new use
    if( (old = atomic_cmpxchg(&(dst_f->active_pid),0,pid)) !=0){
        log("[%d->%d] Error, fiber %d was already in use by %ld\n",tgid,pid,fid,old);
        return ERROR; 
    }
    dbg("Booked dst_fiber %d with active_pid %d",dst_f->fid, atomic_read(&(dst_f->active_pid)));
    
    // Find currently executing fiber, we need to write into it
    src_f = get_fiber_by_id(src_fid, p);
    if(!src_f){ // Currently running fiber does not exist???
        dbg("SwitchToFiber cannot find fiber %ld that was referenced as activated by thread %d\n",src_fid,pid);
        return ERROR;
    }
    dbg("SwitchToFiber, found src_fiber %d has active_pid %d",src_f->fid,atomic_read(&(src_f->active_pid)));
        
    // Save current cpu context into current fiber and mark it as not running
    cpu_regs = task_pt_regs(current); 

    memcpy(&(src_f->pt_regs), cpu_regs, sizeof(struct pt_regs));
    // ****************************************
    // @TODO fpu__save(&f_current->fpu);
    // ***************************************
    
    // Disengage old fiber
    atomic_set(&(src_f->active_pid),0);
    
    dbg("SwitchToFiber, Saved CPU ctx into src_fiber %d and active_pid to 0\n",src_f->fid);
    
    // Restore into the CPU the context of dst_f 
    memcpy(cpu_regs, &(dst_f->pt_regs), sizeof(struct pt_regs));
    
    // ****************************************
    // @TODO RESTORE fpu;
    // ***************************************
    dbg("SwitchToFiber, Loaded into CPU ctx the context that was into dst_fiber %d\n",dst_f->fid);
    dbg("SwitchToFiber, Loaded RIP: %ld\n",dst_f->pt_regs.ip);
    
    atomic_set(&(t->active_fid),dst_f->fid);
    
    return SUCCESS;
}



long kernelFlsAlloc(pid_t tgid, pid_t pid){
    
    pid_t fid;
    
    struct process *p;
    struct thread  *t;
    struct fiber   *f;
    struct fls_free_ll * ll_old;
    long index;
    
    dbg("FlsAlloc, process %d thread %d\n", tgid, pid);
    
    // Check if struct process exists otherwise return error
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error FlsAlloc, [%d->%d] process %d has no fibers yet.\n", tgid, pid, tgid);
        return ERROR;   // In the current process no thread has
                        // been converted to fiber yet
    }
    
    // Check if current thread has been converted to fiber otherwise error
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error FlsAlloc, [%d->%d] thread %d still not in %d->threads\n", tgid, pid, pid, tgid);
        return ERROR;    // Calling thread was not converted yet
    }
        
    fid = atomic_read(&(t->active_fid));
    
    // Find currently executing fiber
    f = get_fiber_by_id(fid, p);
    if (!f){
        dbg("Error FlsAlloc, [%d->%d->%d] currently executing fiber does not exist???\n", tgid, pid, fid);
        return ERROR;    // Target fiber does not exist
    }
    
    // Check if fiber already has used FLS
    if(!f->used_fls){
        
        dbg("FlsAlloc, [%d->%d->%d] fiber had never used FLS, initializing\n", tgid, pid, fid);
        
        // FLS was never used yet, set it up
        // Alloc space
        f->fls = vmalloc(sizeof(long long) * FLS_SIZE);
        
        // Setup LL for free entries
        f->free_ll = vmalloc(sizeof(struct fls_free_ll));
        f->free_ll->index = 1;
        f->free_ll->next = NULL;
        
        dbg("FlsAlloc, [%d->%d->%d] linked list set up\n", tgid, pid, fid);
        
        // Setup the bitmaps
        f->fls_used_bmp = bitmap_alloc(FLS_SIZE, GFP_KERNEL);
        bitmap_clear(f->fls_used_bmp, 0, FLS_SIZE);
        
        f->fls_pointed_bmp = bitmap_alloc(FLS_SIZE, GFP_KERNEL);
        bitmap_clear(f->fls_pointed_bmp, 0, FLS_SIZE);
        
        
        // Bit 1 is pointed by LL - set bit in bitmap
        set_bit(1, f->fls_pointed_bmp);
        
        // Setup bmp
        set_bit(0, f->fls_used_bmp);
        
        dbg("FlsAlloc, [%d->%d->%d] bitmaps set up\n", tgid, pid, fid);
        
        index = 0;
        
        // First intialization complete
        f->used_fls = 1;
        
    } else if(f->free_ll==NULL){ // FLS had already been used, but no
                                 // pointer to free slot is found
                                 
        // -> FLS is full
        dbg("Error FlsAlloc, [%d->%d->%d] no more space is available in FLS\n", tgid, pid, fid);
        return ERROR;
        
    } else { // FLS was already initialized
        
        dbg("FlsAlloc, [%d->%d->%d] fiber has space in FLS\n", tgid, pid, fid);
        
        index = f->free_ll->index;
        
        // Mark slot as used in the bitmap
        set_bit(index, f->fls_used_bmp);
        
        // Clear bit in pointed bmp
        // Slot will not be pointed anymore in any case
        clear_bit(index, f->fls_pointed_bmp);
        
        dbg("FlsAlloc, [%d->%d->%d] got index, updated bitmaps\n", tgid, pid, fid);
        
        // We need to check that
        // + The free bit is not the last one in the bitmap
        // AND
        // + The free bit is not followed by a zone pointed by another LL node
        // AND
        // + Whether the free zone continues or we need the next pointer
        // THEN
        // => if next bit is not pointed and not used, increment
        //      index if it doesn't go above FLS_SIZE
        if(index+1<FLS_SIZE && !test_bit(index+1, f->fls_pointed_bmp) && !test_bit(index+1, f->fls_used_bmp)){
            
            dbg("FlsAlloc, [%d->%d->%d] the slot following the allocated one is free and not pointed by a node of linked list\n", tgid, pid, fid);
            
            f->free_ll->index = index+1;
            set_bit(index+1, f->fls_pointed_bmp);
        
        } else { // Either free zone ends or another LL node points the next
            
            dbg("FlsAlloc, [%d->%d->%d] the slot following the allocated one is either used or pointed by a node of the linked list\n", tgid, pid, fid);
            
            ll_old = f->free_ll;
            f->free_ll = f->free_ll->next;
            vfree(ll_old);
        
        }
        
    }
    
    dbg("FlsAlloc, [%d->%d->%d] Done. Returning index %ld\n", tgid, pid, fid, index);
    
    return index;
}

int kernelFlsFree(pid_t tgid, pid_t pid, long index){

    pid_t fid;
    
    struct process *p;
    struct thread  *t;
    struct fiber   *f;
    struct fls_free_ll * ll_new;
    
    dbg("FlsFree, process %d thread %d\n", tgid, pid);
    
    
    if(index>=FLS_SIZE || index < 0){
        dbg("Error FlsFree, [%d->%d] tried freeing index %ld out of the FLS memory range\n", tgid, pid, index);
        return ERROR;
    }
    
    // Check if struct process exists otherwise return error
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error FlsFree, [%d->%d] process %d has no fibers yet.\n", tgid, pid, tgid);
        return ERROR;   // In the current process no thread has
                        // been converted to fiber yet
    }
    
    // Check if current thread has been converted to fiber otherwise error
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error FlsFree, [%d->%d] thread %d still not in %d->threads\n", tgid, pid, pid, tgid);
        return ERROR;    // Calling thread was not converted yet
    }
        
    fid = atomic_read(&(t->active_fid));
    
    // Find currently executing fiber
    f = get_fiber_by_id(fid, p);
    if (!f){
        dbg("Error FlsFree, [%d->%d->%d] currently executing fiber does not exist???\n", tgid, pid, fid);
        return ERROR;    // Target fiber does not exist
    }
    
    // Check if FLS has been initialized and entry had been previously malloc-ed
    if(!f->used_fls  || !test_bit(index, f->fls_used_bmp)){
        dbg("Error FlsFree, [%d->%d->%d] tried freeing a non malloc-ed entry\n", tgid, pid, fid);
        return ERROR;    // Target entry does not exist
    }
    
    clear_bit(index, f->fls_used_bmp);
    dbg("FlsFree, [%d->%d->%d] usage flag for index %ld cleared\n", tgid, pid, fid, index);
    
    // Freed bit does not follow a free area
    if(!(index > 0 && !test_bit(index-1, f->fls_used_bmp))){
        
        dbg("FlsFree, [%d->%d->%d] cleared bit does not follow a free area, I point it with a linkedList node\n", tgid, pid, fid);
        
        // Create LL node for this new free area
        // Put new node at start of LL chain
        ll_new = vmalloc(sizeof(struct fls_free_ll));
        ll_new->index=index;
        ll_new->next = f->free_ll;
        f->free_ll = ll_new;
        
        dbg("FlsFree, [%d->%d->%d] new head of the linkedList is at index: %ld\n", tgid, pid, fid, f->free_ll->index);
        
        dbg("FlsFree, [%d->%d->%d] next node points index: %ld\n", tgid, pid, fid, f->free_ll->next==NULL ? -1 : f->free_ll->next->index);
        
        // Bit at index index is now pointed by a LL element
        set_bit(index, f->fls_pointed_bmp);
        
    }
    
    // If freed bit is preceeded by a free area, no action is needed, as
    // it will be reached through the LL node pointing to preceding area
    
    dbg("FlsFree, [%d->%d->%d] done!\n", tgid, pid, fid);
    
    return SUCCESS;
}

long long kernelFlsGetValue(pid_t tgid, pid_t pid, long index){
    
    pid_t fid;
    
    struct process *p;
    struct thread  *t;
    struct fiber   *f;
    
    dbg("FlsGetValue, process %d thread %d\n", tgid, pid);
    
    if(index>=FLS_SIZE || index < 0){
        dbg("Error FlsGetValue, [%d->%d] tried reading index %ld out of the FLS memory range\n", tgid, pid, index);
        return ERROR;
    }
    
    // Check if struct process exists otherwise return error
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error FlsGetValue, [%d->%d] process %d has no fibers yet.\n", tgid, pid, tgid);
        return ERROR;   // In the current process no thread has
                        // been converted to fiber yet
    }
    
    // Check if current thread has been converted to fiber otherwise error
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error FlsGetValue, [%d->%d] thread %d still not in %d->threads\n", tgid, pid, pid, tgid);
        return ERROR;    // Calling thread was not converted yet
    }
        
    fid = atomic_read(&(t->active_fid));
    
    // Find currently executing fiber
    f = get_fiber_by_id(fid, p);
    if (!f){
        dbg("Error FlsGetValue, [%d->%d->%d] currently executing fiber does not exist???\n", tgid, pid, fid);
        return ERROR;    // Current fiber does not exist???
    }
    
    // Check if FLS has been initialized and target entry exists
    if(!f->used_fls || !test_bit(index, f->fls_used_bmp)){
        dbg("Error FlsGetValue, [%d->%d->%d] tried accessing a non malloc-ed entry\n", tgid, pid, fid);
        return ERROR;    // Target entry does not exist
    }
    
    dbg("FlsGetValue, [%d->%d->%d] read %lld\n", tgid, pid, fid, f->fls[index]);
    
    return f->fls[index];
}

int kernelFlsSetValue(pid_t tgid, pid_t pid, long index, long long value){
    
    pid_t fid;
    
    struct process *p;
    struct thread  *t;
    struct fiber   *f;
    
    dbg("FlsSetValue, process %d thread %d\n", tgid, pid);
    
    if(index>=FLS_SIZE || index < 0){
        dbg("Error FlsSetValue, [%d->%d] tried writing to index %ld out of the FLS memory range\n", tgid, pid, index);
        return ERROR;
    }
    
    // Check if struct process exists otherwise return error
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error FlsSetValue, [%d->%d] process %d has no fibers yet.\n", tgid, pid, tgid);
        return ERROR;   // In the current process no thread has
                        // been converted to fiber yet
    }
    
    // Check if current thread has been converted to fiber otherwise error
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error FlsSetValue, [%d->%d] thread %d still not in %d->threads\n", tgid, pid, pid, tgid);
        return ERROR;    // Calling thread was not converted yet
    }
        
    fid = atomic_read(&(t->active_fid));
    
    // Find currently executing fiber
    f = get_fiber_by_id(fid, p);
    if (!f){
        dbg("Error FlsSetValue, [%d->%d->%d] currently executing fiber does not exist???\n", tgid, pid, fid);
        return ERROR;    // Current fiber does not exist???
    }
    
    dbg("FlsSetValue, [%d->%d->%d] wants to write %lld in index %ld\n", tgid, pid, fid, value, index);
    
    // Check if FLS has been initialized and target entry has been allocated and not freed
    if(!f->used_fls || !test_bit(index, f->fls_used_bmp)){
        dbg("Error FlsSetValue, [%d->%d->%d] tried writing a non malloc-ed entry\n", tgid, pid, fid);
        return ERROR;    // Target entry does not exist
    }
    
    // Write into the slot
    f->fls[index]=value;
    
    dbg("FlsSetValue, [%d->%d->%d] done\n", tgid, pid, fid);
    
    return SUCCESS;
}


// @TODO IMPLEMENT CLEANUP FUNCTION
void kernelProcCleanup(pid_t tgid){ 

    struct process *p;

    p= get_process_by_id(tgid);


    // Walk into p->threads and free all data
    // Walk into p->fibers and free all data

}

void kernelModCleanup(){
    // Walk into processes and free all data
}
