#include "fibers_driver.h" // For MAJOR_NUM and IOCTL_*
#include "fibers_iface.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// This is the file descriptor needed to isse ioctls,
// It isn't efficient to reopen it f.e. ioctl and it is 
// also usefull to hook cleanup functions on its release
int fd;

pid_t ConvertThreadToFiber(){
    int ret;

    
    fd = open("/dev/"DRIVER_NAME,0,O_RDONLY);
    printf("opened %s, received fd %d.\n","/dev/"DRIVER_NAME,fd);
    
    ret = ioctl(fd,IOCTL_ConvertThreadToFiber,0);
    printf("ret:%d.\n",ret);

    if (ret ==-1 ) perror("ioctl");   

    return 0;
}


pid_t CreateFiber(void (*user_function)(void*),  void * param){
    int ret = ioctl(fd,IOCTL_CreateFiber,0);
    if (ret ==-1 ) perror("ioctl");
    else printf("Ok.");
}

long SwitchToFiber(pid_t pid){
    int ret = ioctl(fd,IOCTL_SwitchToFiber,0);
    if (ret ==-1 ) perror("ioctl");
    else printf("Ok.");

}


/*
long FlsAlloc();
bool FlsFree(long);
long long FlsGetValue(long);
void FlsSetValue(long long, long);
*/
