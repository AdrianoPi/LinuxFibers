#include "fibers_driver.h" // For MAJOR_NUM and IOCTL_*
#include "fibers_iface.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#define STACK_SIZE 1024

#define log(fmt,...) printf( fmt , ##__VA_ARGS__)


// This is the file descriptor needed to issue ioctls,
// It isn't efficient to reopen it f.e. ioctl and it is 
// also usefull to hook cleanup functions on its release
int fd;

int FiberExit(){
    log("Called FiberExit\n");
    return ioctl(fd, IOCTL_FiberExit, 0);
}

pid_t ConvertThreadToFiber(){
    int ret;

    
    fd = open("/dev/"DRIVER_NAME,0,O_RDONLY);
    log("[Fibers Interface] opened %s, fd %d.\n","/dev/"DRIVER_NAME,fd);
    
    ret = ioctl(fd,IOCTL_ConvertThreadToFiber,0);
    log("[Fibers Interface] ret:%d.\n",ret);

    if (ret ==-1 ) perror("[Fibers Interface] ConvertThreadToFiber");

    return ret;
}


pid_t CreateFiber(void (*user_function)(void*),  void * param){
    

    struct fiber_args fargs;   
    fargs.user_fn   = (long) user_function;
    fargs.fn_params = param;
    fargs.stack_size = STACK_SIZE;
    
    // @TODO ADD LIST OF MALLOCed MEM AREAS FOR CLEANUP PURPOSES
    // Otherwise processes with a lot of fibers will end up spraying 
    // the heap.
    //
    // Set stack_base at a 16-memory-aligned address and zero it.
    if (posix_memalign(&(fargs.stack_base), 16, STACK_SIZE)){
        perror("[Fibers Interface] Could not get a memory-aligned stack base!\n");
        return -1;
    }
    bzero(fargs.stack_base, STACK_SIZE);
    
    // @TODO handle fiber return with pthread exit
    long unsigned int * fiberExit_ptr = (long unsigned int*)FiberExit;
    
    
    memcpy((void *) fargs.stack_base+STACK_SIZE-8, &fiberExit_ptr, sizeof(void *));
    printf("bp=%ld,\tfn=%ld,\tequal?=%d\n", *(unsigned long *)(fargs.stack_base+STACK_SIZE-8), FiberExit, ((unsigned long *) * (unsigned long *)(fargs.stack_base+STACK_SIZE-8))== (unsigned long *)FiberExit);
    
    
        
    log("[Fibers Interface] CreateFiber ioctl_param %ld, user_fn %ld\n",
           (long unsigned)&fargs,
           fargs.user_fn); 

    int ret = ioctl(fd,IOCTL_CreateFiber, (long unsigned ) &fargs );
    if (ret ==-1 ) perror("[Fibers Interface] CreateFiber ioctl");
    else           log("[Fibers Interface] CreateFiber Ok.\n");
    
    return ret;

}

pid_t SwitchToFiber(pid_t fiber_id){
    log("[Fibers Interface] SwitchToFiber %d\n", fiber_id); 

    int ret = ioctl(fd,IOCTL_SwitchToFiber,(long unsigned int)fiber_id);
    if (ret ==-1 ) perror("[Fibers Interface] SwitchToFiber ioctl\n");
    else           log("[Fibers Interface] Ok.\n");
    return ret;
}


/* ORIGINAL FUNCTION
long FlsAlloc(){
    log("[Fibers Interface] FlsAlloc\n");
    
    long ret = ioctl(fd, IOCTL_FlsAlloc, 0);
    
    if (ret ==-1 ) log("[Fibers Interface] FlsAlloc ioctl\n");
    else           log("[Fibers Interface] Ok.\n");
    
    return ret;
}
*/

long FlsAlloc(){
    
    long ret = ioctl(fd, IOCTL_FlsAlloc, 0);
    
    if (ret ==-1 ) log("[Fibers Interface] FlsAlloc ioctl\n");
    
    return ret;
}

int FlsFree(long index){
    log("[Fibers Interface] FlsFree %ld\n", index);
    
    int ret = ioctl(fd, IOCTL_FlsFree, index);
    
    if (ret ==-1 ) log("[Fibers Interface] FlsFree ioctl\n");
    else           log("[Fibers Interface] Ok.\n");
    
    return ret;
}

long long FlsGetValue(long index){
    log("[Fibers Interface] FlsGetValue %ld\n", index);
    
    long long ret = ioctl(fd, IOCTL_FlsGetValue, index);
    
    if (ret ==-1 ) log("[Fibers Interface] FlsGetValue ioctl\n");
    else           log("[Fibers Interface] Ok.\n");
    
    return ret;
}

int FlsSetValue(long index, long long value){
    
    struct fls_args flsargs;
    
    log("[Fibers Interface] FlsSetValue %ld<-%lld\n", index, value); 
    
    flsargs.index = index;
    flsargs.value = value;
    
    int ret = ioctl(fd, IOCTL_FlsSetValue, (long unsigned) &flsargs );
    
    if (ret ==-1 ) log("[Fibers Interface] FlsSetValue ioctl\n");
    else           log("[Fibers Interface] Ok.\n");
    
    return ret;
}
