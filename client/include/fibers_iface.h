#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

// Converts the current thread into a Fiber and allows from now on to 
// create other Fibers and switch among them.
pid_t ConvertThreadToFiber();

// Creates a new fiber that we can schedule from now on
// @user_func : function that the Fiber will start executing
// @user_param: the void* pointer to data that will be passed to user_func 
pid_t CreateFiber(void (*user_func)(void*), void *user_param );

// Changes the current context of execution into the one of a given Fiber
// @fiber_id: id of the Fiber that we want to schedule
pid_t SwitchToFiber(pid_t fiber_id);


// Allocates one Fiber Local Storage entry
long FlsAlloc();

// Frees a Fiber Local Storage entry
// @index: index identifier of the entry to be freed
int FlsFree(long index);

// Gets value of a Fiber Local Storage entry
// @index: index identifier of the entry to be read
long long FlsGetValue(long index);

// Sets value of a Fiber Local Storage entry
// @index: index identifier of the entry to be written
// @value: value to be written
int FlsSetValue(long index, long long value);

int FiberExit();

#ifndef FIBERS_DRIVER
#define FIBERS_DRIVER


#include <linux/ioctl.h>


struct fiber_args{
    
    void *stack_base;
    long  stack_size;
     
    long user_fn;
    void *fn_params;

};


struct fls_args{
    
    long index;
    long long value;
    
};


#define DRIVER_NAME       "fibers"
#define MAJOR_NUM         100
#define IOCTL_ConvertThreadToFiber  _IO(MAJOR_NUM, 0)
#define IOCTL_CreateFiber           _IOW(MAJOR_NUM, 1, struct fiber_args * ) 
#define IOCTL_SwitchToFiber         _IOW(MAJOR_NUM, 2, long )

#define IOCTL_FlsAlloc              _IO(MAJOR_NUM, 3)
#define IOCTL_FlsFree               _IOW(MAJOR_NUM, 4, long)
#define IOCTL_FlsGetValue           _IOR(MAJOR_NUM, 5, long)
#define IOCTL_FlsSetValue           _IOW(MAJOR_NUM, 6, struct fls_args *)

#define IOCTL_FiberExit             _IO(MAJOR_NUM, 7)


#endif


