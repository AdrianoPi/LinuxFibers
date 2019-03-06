#ifndef FIBERS_DRIVER
#define FIBERS_DRIVER


#include <linux/ioctl.h>


struct fiber_args{
    void *stack_base;
    long  stack_size;
     
    long user_fn;
    void *fn_params;

    
};

#define DRIVER_NAME       "fibers"
#define MAJOR_NUM         100
#define IOCTL_ConvertThreadToFiber  _IO(MAJOR_NUM, 0)
#define IOCTL_CreateFiber           _IOW(MAJOR_NUM, 1, struct fiber_args * ) 
#define IOCTL_SwitchToFiber         _IOW(MAJOR_NUM, 2, long )

#endif

