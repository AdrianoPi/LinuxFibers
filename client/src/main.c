#include "fibers_iface.h"
#include <stdio.h>
#include <unistd.h>

int mfid=0;

void fiber_fn( void * p ){
    while(1){
        printf("[%d] I'm alive.",(int)p);  
        sleep(1);
        SwitchToFiber(mfid);
    }
}

int main(){ 
    mfid = ConvertThreadToFiber();
    int fid  = CreateFiber(fiber_fn, (void *)1);
    printf("Created Fiber %d\n",fid);
    SwitchToFiber(fid);

    while(1){
        printf("[%d] I'm alive.",mfid);  
        sleep(1);
        SwitchToFiber(fid);
    

    }
}
