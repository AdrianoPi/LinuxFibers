#include "fibers.h"
#include "common.h"

#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>


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

}

// Mantains the responsibility of fibers for each process
struct process{

    DECLARE_HASHTABLE(fibers,6);  // Mantains the list of fibers that are
                                  // activated by a thread that belongs to
                                  // this process for cleanup purposes

    atomic_t last_fid;            // Needed to generate a new fiber id on
                                  // each subsequent CreateFiber()    


    // These attributes are needed to add struct process into an hashtable
    pid_t tgid;               // key for hashtable
    struct hlist_node pnext; // Needed to be added into an hastable
}

DEFINE_HASHTABLE(processes,6)


pid_t kernelConvertThreadToFiber(pid_t tgid,pid_t pid){
    log("kernelConvertThreadToFiber tgid:%d, pid:%d\n",tgid,pid);

    // Create struct process @TODO if not exists (atomically,lock).
    struct process *p = kmalloc(sizeof(struct process),GFP_KERNEL);

    p->tgid = tgid;
    atomic_set(&(p->last_fid),0);
    hash_init(p->fibers);

    hash_add_rcu(processes,&(p->pnext),p->tgid);

    // Create a new fiber, activated by this thread.    
    struct fiber *f = kmalloc(sizeof(struct fiber), GFP_KERNEL);
    
    atomic_set(&(f->active_pid),pid);
    
    f->stack_base = NULL;
    f->stack_size = 0; 
    
    f->fid = atomic_fetch_inc(&(p->last_fid));
    hash_add_rcu(p->fibers,&(f->fnext),f->fid); 
    return 0;
}

pid_t kernelCreateFiber(void (*user_fn)(void *), void *param, pid_t tgid){
    log("kernelCreateFiber\n");

    // Check if struct process with given tgid exists
    struct process *p;
    hash_for_each_possible_rcu(processes, p, pnext, tgid);
    if(p==NULL || p->tgid != tgid ) return -1;


    // Create a new struct fiber with a new stack
    struct fiber *f = kmalloc(sizeof(struct fiber),GFP_KERNEL);

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

