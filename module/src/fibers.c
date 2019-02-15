#include "fibers.h"
#include "common.h"

#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>


struct fiber{

    pid_t           active_tid;   // 0 if there is no active thread
   
 
    spinlock_t      fiber_lock;   // Needed to ensure that no two threads
                                  // try to run the same fiber.
    unsigned long   flags;        // Needed to correctly restore interrupts


    struct pt_regs  regs;         // These two fields will store the state
    struct fpu      fpu;          // of the CPU of this fiber

    void           *stack_base;   // Base of allocated stack, to be freed
    unsigned long   stack_size;   // Size of the allocated stack 


    // These attributes are needed to add struct fiber into an hashtable    
    pid_t             fid;   // key for hashtable
    struct hlist_node fnext; // Needed to be added into an hastable

}


struct process{
    DECLARE_HASHTABLE(fibers,6);


    // These attributes are needed to add struct process into an hashtable
    pid_t pid;               // key for hashtable
    struct hlist_node pnext; // Needed to be added into an hastable
}

DEFINE_HASHTABLE(processes,6)


pid_t kernelConvertThreadToFiber(pid_t tid){
    log("kernelConvertThreadToFiber tid:%d\n",tid);

    
    
    return 0;
}

pid_t kernelCreateFiber(void (*user_fn)(void *), void *param, pid_t tid){
    log("kernelCreateFiber\n");

    return 0;
}

pid_t kernelSwitchToFiber(pid_t fid, pid_t tid){
    log("kernelSwitchToFiber\n");

    return 0;
}

