#include "fibers.h"
#include <asm/fpu/types.h>
#include <asm/fpu/internal.h>

DEFINE_HASHTABLE(processes,6);
DEFINE_SPINLOCK(processes_lock); // processes hashtable spinlock.(RW LOCK?)

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

void freeFiber(struct fiber *f);

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
    snprintf(f->name,30,"%d",f->fid);

    t->active_fid = f->fid;

    // FLS management
    f->used_fls = 0;

    f->entry_point = (void*) task_pt_regs(current)->ip;
    f->parent = pid;
    f->activations = 1;
    atomic_long_set(&(f->failed_activations), 0);
    f->total_running_time = 0;
    f->last_activation_time = current->utime;   // this fiber starts living now
                                                // and is already scheduled


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
    snprintf(f->name,30,"%d",f->fid);

    atomic_set(&(f->active_pid),0);

    f->stack_base = stack_base;
    f->stack_size = stack_size;

    memcpy(&(f->pt_regs), task_pt_regs(current), sizeof(struct pt_regs));
    copy_fxregs_to_kernel(&(f->fpu));
    
     
    f->pt_regs.ip = (long) user_fn;
    f->entry_point = (void *) f->pt_regs.ip;
    //f->pt_regs.cx = (long) user_fn;
    f->pt_regs.di = (long) param;
    f->pt_regs.sp = (long) (stack_base + stack_size) - 8;
    
    f->pt_regs.bp = f->pt_regs.sp;
    
    /*
    // Set return address
    if(copy_to_user((void*)f->pt_regs.bp, &FiberExit, sizeof(void *))){
        dbg("CreateFiber, error could not write in user stack");
    }
    */
    
    // FLS management
    f->used_fls = 0;

    // Additional metrics
    f->parent = pid;
    f->activations = 0;
    atomic_long_set(&(f->failed_activations), 0);
    f->total_running_time = 0;
    f->last_activation_time = 0;    // Gets updated upon switching into it


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
    //unsigned long exectime;
    
    struct fpu *prev_fpu;
    struct fpu *next_fpu;
    struct fxregs_state * next_fx_regs;
    
    log("kernelSwitchToFiber tgid:%d pid:%d fid:%d\n",tgid,pid,fid);

    // Get time spent in userspace
    //exectime = current->utime;
    //dbg("kernelSwitchToFiber [%d->%d] has run last fiber for %lld\n", tgid, pid, current->utime);


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

    src_fid = t->active_fid;

    // Find target fiber
    dst_f = get_fiber_by_id(fid, p);
    if (!dst_f){
        dbg("Error SwitchToFiber, fiber %d not created yet\n",fid);
        return ERROR;    // Target fiber does not exist
    }
    dbg("SwitchToFiber, found dest_fiber %d has active_pid %d\n",fid,atomic_read(&(dst_f->active_pid)));

    // Check if target fiber is already in use and book it for the new use
    if( (old = atomic_cmpxchg(&(dst_f->active_pid),0,pid)) !=0){
        atomic_long_inc(&(dst_f->failed_activations));
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
    
    
    //save previous FPU registers in the previous fiber
    prev_fpu = &(src_f->fpu);
    copy_fxregs_to_kernel(prev_fpu);

    
    //restore next FPU registers of the next fiber
    next_fpu = &(dst_f->fpu);
    next_fx_regs = &(next_fpu->state.fxsave);
    copy_kernel_to_fxregs(next_fx_regs);




    // Update metrics before releasing old fiber
    src_f->total_running_time += current->utime - src_f->last_activation_time;
    dbg("kernelSwitchToFiber [Fiber %ld] total execution time %ld\n", src_fid, src_f->total_running_time);
    
    // Start counting time for new fiber
    dst_f->last_activation_time = current->utime;

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

    t->active_fid = dst_f->fid;

    // Activation successful
    dst_f->activations++;

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

    fid = t->active_fid;

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

    return 0;
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

    fid = t->active_fid;

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

    fid = t->active_fid;

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

    // @TODO COPY TO USER
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

    fid = t->active_fid;

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

int kernelFiberExit(pid_t tgid, pid_t pid){
    
    pid_t fid;
    
    struct process *p;
    struct thread  *t;
    struct fiber   *f;
    //struct fls_free_ll * ll_old;
    
    //int ret;
    
    // Check if struct process exists otherwise return error
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error kernelFiberExit, process %d had no fibers.\n",tgid);
        return ERROR;   // In the current process no thread has
                        // been converted to fiber yet
    }
    
    // Check if current thread has been converted to fiber otherwise error
    t = get_thread_by_id(pid, p);
    if(!t){
        dbg("Error kernelFiberExit, thread %d not in %d->threads\n",pid,tgid);
        return ERROR;    // Calling thread was not converted yet
    }
        
    fid = t->active_fid;
    
    // Find currently executing fiber
    f = get_fiber_by_id(fid, p);
    if (!f){
        dbg("Error kernelFiberExit, currently running fiber %d does not exist?\n",fid);
        return ERROR;    // Currently executing fiber does not exist???
    }
    
    log("kernelFiberExit, [%d->%d->%d] wants to exit, clearing memory...\n", tgid, pid, fid);
    
    freeFiber(f);
    
    /*
     * ALL THIS IS DONE IN freeFiber
     * 
     * 
    // Remove entry from hashtable
    hash_del_rcu(&(f->fnext));
    
    // Free FLS-related fields, if FLS was used
    if(f->used_fls){
        dbg("kernelFiberExit, [%d->%d->%d] used FLS, freeing it\n", tgid, pid, fid);
        dbg("kernelFiberExit, [%d->%d->%d] freeing f->fls\n", tgid, pid, fid);
        vfree(f->fls);

        dbg("kernelFiberExit, [%d->%d->%d] freeing bitmaps\n", tgid, pid, fid);
        bitmap_free(f->fls_used_bmp);
        bitmap_free(f->fls_pointed_bmp);
        //kfree(f->fls_used_bmp);
        //kfree(f->fls_pointed_bmp);
        dbg("kernelFiberExit, [%d->%d->%d] freeing f->free_ll\n", tgid, pid, fid);
        while(f->free_ll){
            ll_old = f->free_ll;
            f->free_ll = f->free_ll->next;
            vfree(ll_old);
        }
    } else {
        dbg("kernelFiberExit, [%d->%d->%d] had never used FLS\n", tgid, pid, fid);
    }
    
    
    // Free struct fiber itself
    kfree(f);
    */
    
    
    // DO NOT free Thread entry unless the process is exiting
    // as other threads may want to call the thread's fibers
    
    log("kernelFiberExit, [%d->%d->%d] done! Fiber exiting...\n", tgid, pid, fid);
    do_exit(0);
    
}

void freeFiber(struct fiber *f){
    struct fls_free_ll * ll_old;
    
    // Free fiber stack?
    
    // Delete entry from hashtable
    hash_del_rcu(&(f->fnext));
    
    // Free FLS-related fields, if FLS was used
    if(f->used_fls){
        dbg("freeFiber, [%d] used FLS, freeing it\n", f->fid);
        dbg("freeFiber, [%d] freeing f->fls\n", f->fid);
        vfree(f->fls);

        dbg("freeFiber, [%d] freeing bitmaps\n", f->fid);
        bitmap_free(f->fls_used_bmp);
        bitmap_free(f->fls_pointed_bmp);

        dbg("freeFiber, [%d] freeing f->free_ll\n", f->fid);
        while(f->free_ll){
            ll_old = f->free_ll;
            f->free_ll = f->free_ll->next;
            vfree(ll_old);
        }
    } else {
        dbg("freeFiber, [%d] had never used FLS\n", f->fid);
    }
    
    // Free struct fiber itself
    dbg("freeFiber, [%d] freeing the struct fiber itself\n", f->fid);
    kfree(f);
}


// Cleanup function when process exits
void kernelProcCleanup(pid_t tgid){ 

    struct process  *p;
    struct thread   *t;
    struct fiber    *f;
    int bucket;

    log("kernelProcCleanup for process %d\n",tgid);
    
    p = get_process_by_id(tgid);
    if(!p){
        dbg("Error kernelProcCleanup, process %d had no fibers.\n",tgid);
        return;
    }
    
    // Iterate over all fibers in the table of p
    hash_for_each_rcu(p->fibers, bucket, f, fnext){
        if (f==NULL) break; // Cleaned all fibers
        
        dbg("kernelProcCleanup, freeing fiber %d.\n", f->fid);
        
        // Cleanup after the fiber
        freeFiber(f);
    }
    
    // Iterate over all threads in the table of p
    hash_for_each_rcu(p->threads, bucket, t, tnext){
        if(t==NULL) break; // Cleaned all threads
        
        dbg("kernelProcCleanup, freeing thread %d.\n", t->pid);
        
        // Delete entry from hashtable
        hash_del_rcu(&(t->tnext));
        // Free the struct thread itself
        kfree(t);
    }
    
    // Remove the process entry from hashtable
    hash_del_rcu(&(p->pnext));
    // Free the struct process itself
    kfree(p);
    
    return;

}

void kernelModCleanup(){
    
    struct process  *p;
    int bucket=0;
    
    dbg("kernelModCleanup removing everything\n");
    
    // Walk into processes and free all data
    hash_for_each_rcu(processes, bucket, p, pnext){
        if(p==NULL){ // Cleaned all threads
            dbg("kernelModCleanup all entries for all processes removed\n");
            break;
        }
        dbg("kernelModCleanup found process %d, cleaning up\n", p->tgid);
        kernelProcCleanup(p->tgid);
    }
    
    dbg("kernelModCleanup done.\n");
}
