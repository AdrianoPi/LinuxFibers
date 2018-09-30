#define MAJOR_NUM 230

#define IOCTL_PRINT_HELLO _IOR(MAJOR_NUM,100, unsigned long)
#define IOCTL_CHANGE_IP _IOR(MAJOR_NUM,101, unsigned long)

