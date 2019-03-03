#include "fibers.h"
#include "common.h"

#include <linux/slab.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <asm/thread_info.h>

#define task_stack_page(tsk)    ((void *)(tsk))

// @TODO UPON `return ERROR` ADD LOG()

// Mantains the cpu context associated with the workflow of this fiber.
struct fiber{

    atomic_t           active_pid;   // 0 if there is no active thread, ensure
                                     // mutual exclusion from SwitchToFiber 
    
    /* @TODO set active_pid atomically and avoid usage of this spinlock
    spinlock_t      fiber_lock;   // Needed to ensure that no two threads
                                  // try to run the same fiber.
    unsigned long   flags;        // Needed to correctly restore interrupts
    */ 

    struct pt_regs  pt_regs;         // These two fields will store the state
    struct fpu      fpu;          // of the CPU of this fiber

    void           *stack_base;   // Base of allocated stack, to be freed
    unsigned long   stack_size;   // Size of the allocated stack 


    // These attributes are needed to add struct fiber into an hashtable    
    pid_t             fid;   // key for hashtable
    struct hlist_node fnext; // Needed to be added into an hastable

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

pid_t kernelConvertThreadToFiber(pid_t tgid,pid_t pid){
    struct process *p;
    struct thread  *t;
    struct fiber   *f;

    unsigned long flags;

    log("kernelConvertThreadToFiber tgid:%d, pid:%d\n",tgid,pid);
    
    
    // Access bucket with key tgid and search for the struct process
    spin_lock_irqsave(&processes_lock,flags);

    hash_for_each_possible_rcu(processes, p, pnext, tgid){
        if(p==NULL) break;
        if(p->tgid==tgid) break;
    }

    if(p==NULL){    // First thread of process that is going to be 
                    //converted to Fiber.
        dbg("There was no process %d in the hashtable, lets create one.\n",tgid);

        p= kmalloc(sizeof(struct process),GFP_KERNEL);

        p->tgid = tgid;
        atomic_set(&(p->last_fid),0);
        hash_init(p->fibers);
        hash_init(p->threads);
        hash_add_rcu(processes,&(p->pnext),p->tgid);

    }

    spin_unlock_irqrestore(&processes_lock,flags);

    // Create a new thread struct only if it doesn't still exists
    hash_for_each_possible_rcu(p->threads, t, tnext, pid){
        if(t==NULL) break;
        if(t->pid==pid) {
            dbg("Error converting thread %d to fiber, it already exists in p->threads.\n",pid);
            return ERROR; // Thread is already a Fiber
        }
    }

    
    t= kmalloc(sizeof(struct thread),GFP_KERNEL);
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

    dbg("A new fiber with fid %d is created, with active_pid %d\n",f->fid,atomic_read(&(f->active_pid)));

    hash_add_rcu(p->fibers,&(f->fnext),f->fid); 

    return f->fid;
}

pid_t kernelCreateFiber(long user_fn, void *param, pid_t tgid,pid_t pid, void *stack_base, size_t stack_size){


    struct process *p;
    struct thread  *t;
    struct fiber   *f;

    log("kernelCreateFiber\n");

    // Check 
    //   if struct process with given tgid exists or
    //   if struct thread with given pid exists
    hash_for_each_possible_rcu(processes, p, pnext, tgid){
        if ( p==NULL ) {
            dbg("Error creating fiber, process %d still not created into processes hashtable",tgid);
            return ERROR;
        }
        if ( p->tgid==tgid ) break;
    }

    hash_for_each_possible_rcu(p->threads, t, tnext,pid){
        if(t == NULL){ 
            dbg("Error creating fiber, thread %d still not created into %d->threads",pid,tgid);
            return ERROR;
        }
        if(t->pid == pid ) break;
    }

    // Create a new struct fiber with given function and stack
    // Initially registers are not set because they are needed to store 
    // data when a running fiber is scheduled out, only rip is set.

    f= kmalloc(sizeof(struct fiber),GFP_KERNEL);
    f->fid = atomic_fetch_inc(&(p->last_fid)) ;
    atomic_set(&(f->active_pid),0);

    f->stack_base = stack_base;
    f->stack_size = stack_size;
    
    memcpy(&(f->pt_regs), task_pt_regs(current), sizeof(struct pt_regs));
    f->pt_regs.ip = (long) user_fn;
    f->pt_regs.cx = (long) user_fn;
    f->pt_regs.di = (long) param;
    f->pt_regs.sp = (long) (stack_base + stack_size) - 8; 
    f->pt_regs.bp = f->pt_regs.sp;
    //*((long*)(f->pt_regs.sp)) = (long)NULL; 
   
    dbg("Inserting a new fiber fid %d with active_pid %d and RIP %ld",f->fid,atomic_read(&(f->active_pid)),(long)f->pt_regs.ip);
     
    hash_add_rcu(p->fibers,&(f->fnext),f->fid);
    
    // Maybe call SwitchToFiber(?)

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
    hash_for_each_possible_rcu(processes, p, pnext, tgid){

        if(p==NULL){
            dbg("Error SwitchToFiber, process %d still not created.\n",tgid);
            return ERROR;        // In the current process no thread has
        }                        // been converted to fiber yet
        if(p->tgid==tgid) break;
    }

    
    // Check if current thread has been converted to fiber otherwise error
    hash_for_each_possible_rcu(p->threads, t, tnext, pid){

        if(t==NULL){
            dbg("Error SwitchToFiber, thread %d still not in %d->threads\n",pid,tgid);
            return ERROR;    // Calling thread was not converted yet
       }
       if(t->pid==pid) break;
    }
    src_fid=atomic_read(&(t->active_fid));
    
    // Find target fiber
    hash_for_each_possible_rcu(p->fibers, dst_f, fnext, fid){
        if(dst_f==NULL){
            dbg("Error SwitchToFiber, fiber %d still not created\n",fid);
            return ERROR;    // Target fiber does not exist
       }
       if(dst_f->fid==fid) break;
    }
    dbg("SwitchToFiber, found dest_fiber %d has active_pid %d\n",fid,atomic_read(&(dst_f->active_pid)));

    // Check if target fiber is already in use and book it for the new use
    if( (old = atomic_cmpxchg(&(dst_f->active_pid),0,pid)) !=0){
        log("[%d->%d] Error, fiber %d was already in use by %ld\n",tgid,pid,fid,old);
        return ERROR; 
    }
    dbg("Booked dst_fiber %d with active_pid %d",dst_f->fid, atomic_read(&(dst_f->active_pid)));
    
    dbg("Searching src_fiber %ld into p->[%p]",src_fid,p->fibers); 
    // Find currently executing fiber, we need to write into it
    hash_for_each_possible_rcu(p->fibers, src_f, fnext, src_fid){
        if(src_f==NULL){
            dbg("SwitchToFiber cannot find fiber %ld that was referenced as activated by thread %d\n",src_fid,pid);
            return ERROR;    // src_f does not exist???
       }
       if(src_f->fid==src_fid) break;
    }
    dbg("SwitchToFiber, found src_fiber %d has active_pid %d",src_f->fid,atomic_read(&(src_f->active_pid)));
        
    // Save current cpu context into current fiber and mark it as not running
    cpu_regs = task_pt_regs(current); 

    memcpy(&(src_f->pt_regs), cpu_regs, sizeof(struct pt_regs));
    // ****************************************
    // @TODO fpu__save(&f_current->fpu);
    // ***************************************
    atomic_set(&(src_f->active_pid),0);
    
    dbg("SwitchToFiber, Saved CPU ctx into src_fiber %d and active_pid to 0\n",src_f->fid);
    
    // Restore into the CPU the context of dst_f 
    memcpy(cpu_regs, &(dst_f->pt_regs), sizeof(struct pt_regs));
    // ****************************************
    // @TODO RESTORE fpu;
    // ***************************************
    dbg("SwitchToFiber, Loaded into CPU ctx the context that was into dst_fiber %d\n",dst_f->fid);
    dbg("SwitchToFiber, Loaded RIP: %ld\n",dst_f->pt_regs.ip);
    return SUCCESS;
}
