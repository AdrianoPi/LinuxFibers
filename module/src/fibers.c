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

    if(p==NULL){    // There is no other thread of the current tgid
                    // already converted to Fiber.

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
       if(t->pid==pid) return ERROR; // Thread is already a Fiber
    }
    
    t= kmalloc(sizeof(struct thread),GFP_KERNEL);
    t->pid=pid;
    hash_add_rcu(p->threads,&(t->tnext),t->pid);



    // Create a new fiber, activated by this thread.    
    f= kmalloc(sizeof(struct fiber), GFP_KERNEL);
    
    atomic_set(&(f->active_pid),pid);
    
    f->stack_base = NULL; // A Fiber created from an existing Thread
    f->stack_size = 0;    // has not a newly allocated stack
    
    f->fid = atomic_fetch_inc(&(p->last_fid));
    atomic_set(&(t->active_fid),f->fid);

    hash_add_rcu(p->fibers,&(f->fnext),f->fid); 
    
    

    return SUCCESS;
}

pid_t kernelCreateFiber(void (*user_fn)(void *), void *param, pid_t tgid,pid_t pid, void *stack_base, size_t stack_size){

    struct process *p;
    struct thread  *t;
    struct fiber   *f;

    log("kernelCreateFiber\n");

    // Check 
    //   if struct process with given tgid exists or
    //   if struct thread with given pid exists
    hash_for_each_possible_rcu(processes, p, pnext, tgid){
        if ( p==NULL ) return ERROR;
        if ( p->tgid==tgid ) break;
    }

    hash_for_each_possible_rcu(p->threads, t, tnext,pid){
        if(t == NULL) return ERROR;
        if(t->pid == pid ) break;
    }

    // Create a new struct fiber with given function and stack
    // Initially registers are not set because they are needed to store 
    // data when a running fiber is scheduled out, only rip is set.

    f= kmalloc(sizeof(struct fiber),GFP_KERNEL);
    f->fid = atomic_fetch_inc(&(p->last_fid)) ;

    f->stack_base = stack_base;
    f->stack_size = stack_size;
    
    memcpy(&(f->pt_regs), task_pt_regs(current), sizeof(struct pt_regs));
    f->pt_regs.ip = (long) user_fn;
    f->pt_regs.di = (long) param;
    f->pt_regs.sp = (long) (stack_base + stack_size) - 8; 
    f->pt_regs.bp = f->pt_regs.sp;

    f->pt_regs.sp = (long) NULL; 
    
    hash_add_rcu(p->fibers,&(f->fnext),f->fid);
    
    // Maybe call SwitchToFiber(?)

    return 0;
}

pid_t kernelSwitchToFiber(pid_t tgid, pid_t pid, pid_t fid){
    
    struct process *p;
    struct thread  *t;
    struct fiber   *f;
    struct fiber   *f_current;
    
    struct pt_regs *regs;
    regs = NULL;
    
    log("kernelSwitchToFiber\n");

    // Check if struct process exists otherwise return error
    hash_for_each_possible_rcu(processes, p, pnext, tgid){
        if(p==NULL) return ERROR;   // In the current process no thread has
                                    // been converted to fiber yet
        
        if(p->tgid==tgid) break;
    }
    
    // Check if current thread has been converted to fiber otherwise error
    hash_for_each_possible_rcu(p->threads, t, tnext, pid){
       if(t==NULL) return ERROR;    // Calling thread was not converted yet
       
       if(t->pid==pid) break;
    }
    
    // TODO: Error if target fiber is the current fiber?
    // if(t->active_fiber==fid) return ERROR;

    
    // Find currently executing fiber, we need to write into this later
    hash_for_each_possible_rcu(p->fibers, f_current, fnext, fid){
       if(f_current==NULL) return ERROR;    // Current f does not exist???
       
       if(f_current->fid==atomic_read(& t->active_fid)) break;
    }
        
    // Find target fiber
    hash_for_each_possible_rcu(p->fibers, f, fnext, fid){
       if(f==NULL) return ERROR;    // Target fiber does not exist
       
       if(f->fid==fid) break;
    }
    
    // Save current cpu context into current fiber
    memcpy(&(f->pt_regs), task_pt_regs(current), sizeof(struct pt_regs));
    // fpu__save(&f_current->fpu);
    
    
    // Restore into the cpu the context saved in the target fiber (id=fid)



    return 0;
}



