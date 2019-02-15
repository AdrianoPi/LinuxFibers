#include "fibers.h"
#include "common.h"

pid_t kernelConvertThreadToFiber(pid_t tid){
    log("kernelConvertThreadToFiber\n");
    
    return 0;
}

pid_t kernelCreateFiber(void (*user_fn)(void *), void *param, pid_t tid){
    log("kernelCreateFiber\n");

    return 0;
}

pid_t kernelSwitchToFiber(pid_t fid, pid_t tid){
    log("kernelSwitchToFiber\n");

    return 0;
}

