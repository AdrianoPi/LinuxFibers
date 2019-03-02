#include "fibers_iface.h"
#include <stdio.h>
#include <unistd.h>

int mfid=0;

void fiber_fn( void * p ){
    while(1){
        printf("[%ld] I'm aliveU.\n",(long int)p);  
        sleep(1);
        SwitchToFiber(mfid);
    }
}

int main(){ 
    mfid = ConvertThreadToFiber();
    printf("Creating fiber with RIP:%p\n",fiber_fn);

    int fid  = CreateFiber(fiber_fn, (void *)1);
    printf("Created Fiber %d\n",fid);
    SwitchToFiber(fid);

    while(1){
        printf("[%d] I'm aliveA.\n",mfid);  
        sleep(1);
        SwitchToFiber(fid);
    

    }
}
