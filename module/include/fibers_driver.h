#ifndef FIBERS_DRIVER
#define FIBERS_DRIVER


#include <linux/ioctl.h>


#define DRIVER_NAME       "fibers"
#define MAJOR_NUM         100
#define IOCTL_ConvertThreadToFiber  _IOR(MAJOR_NUM, 0, void *) 
#define IOCTL_CreateFiber           _IOR(MAJOR_NUM, 1, void *) 
#define IOCTL_SwitchToFiber         _IOR(MAJOR_NUM, 2, void *)

struct fiber_args{
    void *user_fn;

};

#endif

