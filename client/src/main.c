#include "fibers_iface.h"
#include "tests.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define SUCCESS     0
#define ERROR       -1

// We will test Fibers with three instances
int fid0=0;
int fid1=0;
int fid2=0;

// This will be the function of the Fibers
void fiber_fn( void * p ){
    while(1){
        printf("[FIBER-%ld] I'm an alive.\n",(long int)p);
        SwitchToFiber(fid0);
    }
}

int main(){
    int ret;
    long index;
    
    //flsAllocSetGetFree();
    
    // Convert the main thread to a Fiber
    fid0 = ConvertThreadToFiber();
    
    /*
    ret = FlsAlloc_test_01();
    print_test_outcome(ret, "FlsAlloc_test_01");
    printf("\n");
    */
    
    ret = flsAlloc_Until_err();
    
    /*ret = FlsFree(50);
    print_test_outcome(ret, "FlsFree");
    printf("\n");*/
    
    /*ret = flsAllocSetGetFree();
    print_test_outcome(ret, "FlsAllocSetGetFree");
    printf("\n");*/
    
    
    return 0;
    
    // Create another fiber fiber0
    printf("Creating fiber with RIP:%p\n",fiber_fn);
    fid1  = CreateFiber(fiber_fn, (void *)1);
    if(fid1==-1){
        printf("Error creating fiber.\n");
        exit(1);
    }
    printf("Created Fiber %d\n",fid1);

    // Create another fiber fiber1
    printf("Creating fiber with RIP:%p\n",fiber_fn);
    fid2  = CreateFiber(fiber_fn, (void *)2);
    if(fid2==-1){
        printf("Error creating fiber.\n");
        exit(1);
    }
    printf("Created Fiber %d\n",fid2);

    // Main Fibers scheduler
    while(1){
        printf("****\n");
        printf("[MAIN-%d] I'm aliveA.\n",fid0);  
        sleep(1);
        SwitchToFiber(fid1);
        sleep(1);
        SwitchToFiber(fid2);
        sleep(1);
    }
    
}
