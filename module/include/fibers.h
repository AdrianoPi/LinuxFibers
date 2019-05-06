#ifndef FIBERS_FIBERSH
#define FIBERS_FIBERSH

#include "common.h"

#include <linux/slab.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <asm/thread_info.h>
#include <linux/sched/task_stack.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/time.h>
#include <linux/hashtable.h>


pid_t kernelConvertThreadToFiber    (pid_t tgid, \
                                    pid_t pid);  \

pid_t kernelCreateFiber             (long user_fn,      \
                                    void *param,        \
                                    pid_t tgid,         \
                                    pid_t pid,          \
                                    void *stack_base,   \
                                    size_t stack_size);

pid_t kernelSwitchToFiber           (pid_t tgid, \
                                    pid_t pid,   \
                                    pid_t fid);


long kernelFlsAlloc                 (pid_t tgid, \
                                    pid_t pid);

int kernelFlsFree                   (pid_t tgid,          \
                                    pid_t pid,            \
                                    long index);

long long kernelFlsGetValue         (pid_t tgid,          \
                                    pid_t pid,            \
                                    long index);

int kernelFlsSetValue               (pid_t tgid,          \
                                    pid_t pid,            \
                                    long index,           \
                                    long long value);
                                    
int kernelFiberExit                 (pid_t tgid,          \
                                    pid_t pid);

void kernelProcCleanup (pid_t tgid);
void kernelModCleanup  (void);


#define FLS_SIZE 4096

// To keep track of free slots in FLS
struct fls_free_ll{

    long index;

    struct fls_free_ll * next;
};

// Mantains the cpu context associated with the workflow of this fiber.
struct fiber{

    atomic_t        active_pid;   // 0 if there is no active thread, ensure
                                  // mutual exclusion from SwitchToFiber

    struct pt_regs  pt_regs;      // These two fields will store the state
    struct fpu      fpu;          // of the CPU of this fiber

    void           *stack_base;   // Base of allocated stack, to be freed
    unsigned long   stack_size;   // Size of the allocated stack


    // These attributes are needed to add struct fiber into an hashtable
    pid_t             fid;   // key for hashtable
    struct hlist_node fnext; // Needed to be added into an hastable


    // FLS-related fields

    long long * fls;
    // Bitmap to check for used slots
    unsigned long * fls_used_bmp;

    // LinkedList to keep slots inbetween
    struct fls_free_ll * free_ll;
    // Bitmap to check if a slot is pointed by a LL node
    unsigned long * fls_pointed_bmp;

    int used_fls;

    // Metrics
    char           name[50];

    pid_t           parent;       // Pid of thread that created the fiber

    unsigned long   activations;  // Successful activation is guarded
                                  // don't need atomic

    atomic_long_t   failed_activations; // Needs to be atomic if fibers
                                        // are not thread-specific

    unsigned long   total_running_time;
    unsigned long   last_activation_time;

    void * entry_point;

};

// Mantains the responsibility of fibers for each process
struct process{

    DECLARE_HASHTABLE(fibers,6);  // Mantains the list of fibers that are
                                  // activated by a thread that belongs to
                                  // this process for cleanup purposes.

    DECLARE_HASHTABLE(threads,6); // Needed to check if a given thread has
                                  // already been converted to fiber.

    atomic_t last_fid;      // Needed to generate a new fiber id on
                                  // each subsequent CreateFiber().


    // These attributes are needed to add struct process into an hashtable
    pid_t tgid;               // key for hashtable
    struct hlist_node pnext;  // Needed to be added into an hastable
};

// Mantain thread activated fiber
struct thread{

    pid_t active_fid;

    // These attributes are needed to add struct process into an hashtable
    pid_t pid;                // key for hashtable
    struct hlist_node tnext;  // Needed to be added into an hastable
};


// get_x_by_id are auxiliary functions
// If no matching entry is found, they return NULL
inline struct process * get_process_by_id(pid_t tgid);

inline struct thread * get_thread_by_id(pid_t pid, struct process * p);

inline struct fiber * get_fiber_by_id(pid_t fid, struct process * p);


#endif
