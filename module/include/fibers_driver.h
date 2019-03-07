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
#define IOCTL_FlsFree               _IOW(MAJOR_NUM, 4, unsigned long)
#define IOCTL_FlsGetValue           _IOR(MAJOR_NUM, 5, unsigned long)
#define IOCTL_FlsSetValue           _IOW(MAJOR_NUM, 6, struct fls_args *)


#endif

