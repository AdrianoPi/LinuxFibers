#include "fibers_driver.h" // For MAJOR_NUM and IOCTL_*
#include "fibers_iface.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

pid_t ConvertThreadToFiber(){
    int ret;

    printf("IOCTL:%d\n",IOCTL_PRINT_MSG);
    
    int fd = open("/dev/"DRIVER_NAME,0,O_RDONLY);
    printf("opened %s, received fd %d.\n","/dev/"DRIVER_NAME,fd);
    
    ret = ioctl(fd,IOCTL_PRINT_MSG,0);
    printf("ret:%d.\n",ret);

    if (ret ==-1 ) perror("ioctl");   

    return 0;
}

/*
pid_t CreateFiber(user_function_t, unsigned long, void *);
long SwitchToFiber(pid_t);
long FlsAlloc();
bool FlsFree(long);
long long FlsGetValue(long);
void FlsSetValue(long long, long);
*/
