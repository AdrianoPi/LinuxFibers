#include "fibers_driver.h" // For MAJOR_NUM and IOCTL_*
#include "fibers_iface.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define STACK_SIZE 1024


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

    return ret;
}


pid_t CreateFiber(void (*user_function)(void*),  void * param){
    
    // @TODO ADD LIST OF MALLOCed MEM AREAS FOR CLEANUP PURPOSES
    struct fiber_args *fargs = malloc(sizeof(struct fiber_args));
   
    fargs->user_fn   = user_function;
    fargs->fn_params = param;
     
    fargs->stack_base = malloc(STACK_SIZE);
    fargs->stack_size = STACK_SIZE;
    
    printf("Create Fiber user_fn %p user_param %p\n",user_function,param); 
    int ret = ioctl(fd,IOCTL_CreateFiber, (long ) fargs );
    if (ret ==-1 ) perror("ioctl");
    else printf("Ok.");
    return ret;
}

long SwitchToFiber(pid_t fiber_id){
    int ret = ioctl(fd,IOCTL_SwitchToFiber,(long unsigned int)fiber_id);
    if (ret ==-1 ) perror("ioctl");
    else printf("Ok.");

}


/*
long FlsAlloc();
bool FlsFree(long);
long long FlsGetValue(long);
void FlsSetValue(long long, long);
*/
