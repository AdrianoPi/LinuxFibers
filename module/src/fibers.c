#include "fibers.h"
#include "common.h"

#include <linux/slab.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>
#include <linux/sched.h>

// Mantains the cpu context associated with the workflow of this fiber.
struct fiber{

    atomic_t           active_pid;   // 0 if there is no active thread, ensure
                                     // mutual exclusion from SwitchToFiber 
    
    /* @TODO set active_pid atomically and avoid usage of this spinlock
    spinlock_t      fiber_lock;   // Needed to ensure that no two threads
                                  // try to run the same fiber.
    unsigned long   flags;        // Needed to correctly restore interrupts
    */ 

    struct pt_regs  regs;         // These two fields will store the state
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
    pid_t pid;               // key for hashtable
    struct hlist_node tnext;  // Needed to be added into an hastable
}
};

DEFINE_HASHTABLE(processes,6);


pid_t kernelConvertThreadToFiber(pid_t tgid,pid_t pid){
    struct process *p;
    struct fiber *f; 
    log("kernelConvertThreadToFiber tgid:%d, pid:%d\n",tgid,pid);
    

    //@TODO CS ***************************

    // Access bucket with key tgid and search for the struct process
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
        hash_add_rcu(processes,&(p->pnext),p->tgid);

    }
    // END CS *****************************

    struct thread *t;
    hash_for_each_possible_rcu(p->threads, t, tnext, pid){
       if(t==NULL) break;
       if(t->pid==pid) return ERROR; // Thread is already a Fiber
    }
    
    t= kmalloc(sizeof(struct thread),GFP_KERNEL);
    t->pid=pid;
    t->active_fid = MAKE-CRASHPLEASE;



    // Create a new fiber, activated by this thread.    
    f= kmalloc(sizeof(struct fiber), GFP_KERNEL);
    
    atomic_set(&(f->active_pid),pid);
    
    f->stack_base = NULL;
    f->stack_size = 0; 
    
    f->fid = atomic_fetch_inc(&(p->last_fid));
    hash_add_rcu(p->fibers,&(f->fnext),f->fid); 
    return 0;
}

pid_t kernelCreateFiber(void (*user_fn)(void *), void *param, pid_t tgid){
    struct fiber *f;
    struct process *p;

    log("kernelCreateFiber\n");

    // Check if struct process with given tgid exists
    hash_for_each_possible_rcu(processes, p, pnext, tgid);
    if(p==NULL || p->tgid != tgid ) return -1;


    // Create a new struct fiber with a new stack

    f= kmalloc(sizeof(struct fiber),GFP_KERNEL);
    f->fid = atomic_fetch_inc(&(p->last_fid));

    atomic_set(&(f->active_pid),current->pid);

    f->stack_base = kmalloc(1024,GFP_USER);
    f->stack_size = 1024;

    // @TODO USER_FN LAYOUT 
    //f->stack_base[1024-4]=*(param); // 1st param
    //f->stack_base[1024-8]=NULL // ret addr
    // f->sp = f->stack_base[1024-8]

    hash_add_rcu(p->fibers,&(f->fnext),f->fid);
    
    // Maybe call SwitchToFiber(?)

    return 0;
}

pid_t kernelSwitchToFiber(pid_t fid, pid_t pid){
    log("kernelSwitchToFiber\n");

    // Check if struct process exists otherwise return error
    // Find the fiber that was activated by such tid and
    //     save the current cpu context into this fiber
    // Restore into the cpu the context saved into the fiber fid



    return 0;
}

