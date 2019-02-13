#ifndef FIBERS_DRIVER
#define FIBERS_DRIVER


#include <linux/ioctl.h>


#define DRIVER_NAME       "fibers"
#define MAJOR_NUM         100
#define IOCTL_PRINT_MSG   _IOR(MAJOR_NUM, 0, void *) // major, idx, paramtype


#endif

