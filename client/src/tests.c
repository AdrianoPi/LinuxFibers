#include "fibers_iface.h"
#include "tests.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define SUCCESS     0
#define ERROR       -1


void print_test_outcome(int ret, char* test_name){
    if(ret==SUCCESS){
        printf("\n%s - SUCCESSFUL\n", test_name);
    } else {
        printf("\n%s - FAILED\n", test_name);
    }
}

// We try alloc-ing all possible slots, and check that the next gives error
// Tests a non-initialized FLS
// Tests initialized FLS with space available
// Tests initialized FLS without any more space
int FlsAlloc_test_01(){
    printf("\n\nFlsAlloc_test_01 - Will try alloc-ing all possible address and one more\n\n");
    int successes=0;
    int tries = 0;
    int to_alloc=4096;
    long addr;
    do{
        addr=FlsAlloc();
        tries++;
        if(addr!=ERROR) successes++;
    }while(tries<to_alloc);
    printf("\n\nFlsAlloc_test_02 - Allocated %d slots. Got %d successful allocations\n\n", to_alloc, successes);
    addr=FlsAlloc();
    if(addr== (long) ERROR){
        return SUCCESS;
    }
    return ERROR;
}


// VC01 - Try freeing a valid address
int FlsFree_test_01(){
    
    long addr_to_free = 5;
    printf("\n\nFlsFree_test_01 - Will try freeing TLS address %ld\n\n", addr_to_free);
    
    int ret = FlsFree(addr_to_free);
    
    return ret;
}

// IC02 - Free invalid address -1
int FlsFree_test_02(){
    
    long addr_to_free = -1;
    printf("\n\nFlsFree_test_02 - Will try freeing illegal TLS address %ld\n\n", addr_to_free);
    
    int ret = FlsFree(addr_to_free);
    
    if(ret == ERROR){
        return SUCCESS;
    }
    return ERROR;
    
}

// IC03 - Free invalid address FLS_SIZE
int FlsFree_test_03(){
    
    long addr_to_free = 4096;
    printf("\n\nFlsFree_test_03 - Will try freeing illegal TLS address %ld\n\n", addr_to_free);
    
    int ret = FlsFree(addr_to_free);
    
    if(ret == ERROR){
        return SUCCESS;
    }
    return ERROR;

}

// BVA for FlsFree want to test -1, 0, 1, FLS_SIZE-1, FLS_SIZE, FLS_SIZE+1
int FlsFree_test_BVA(){
    // invalid index -1 and FLS_SIZE are checked in IC02 and IC03
    long addresses_to_free[4]={0, 1, 4095, 4097};
    int ret;
    
    for(int i=0; i<4; i++){
        printf("\n\nFlsFree_test_BVA - Will try freeing TLS address %ld\n\n", addresses_to_free[i]);
        ret |= FlsFree(addresses_to_free[i]);
    }
    
    if(ret == ERROR){
        return SUCCESS;
    }
    return ERROR;
    
}

int tests_with_fiber(){
    int ret;
    int all_rets=0;
    
    ret = FlsAlloc_test_01();
    print_test_outcome(ret, "FlsAlloc_test_01");
    all_rets|=ret;
    
    ret = FlsFree_test_01();
    print_test_outcome(ret, "FlsFree_test_01");
    all_rets|=ret;
    
    printf("Tried allocating after freeing, got back address %ld\n", FlsAlloc());
    
    ret = FlsFree_test_02();
    print_test_outcome(ret, "FlsFree_test_02");
    all_rets|=ret;
    
    ret = FlsFree_test_03();
    print_test_outcome(ret, "FlsFree_test_03");
    all_rets|=ret;
    
    ret = FlsFree_test_BVA();
    print_test_outcome(ret, "FlsFree_test_BVA");
    all_rets|=ret;
    
    return all_rets;
}

void Fls_Alloc_Free_Test(){
    int ret;
    long index;
    ret = FlsAlloc_test_01();
    print_test_outcome(ret, "FlsAlloc_test_01");
    
    // Now the full FLS is allocated. We free some slots in order
    ret = FlsFree(5);
    printf("Tried freeing index 5, result %d\n", ret==SUCCESS);
    
    ret = FlsFree(9);
    printf("Tried freeing index 9, result %d\n", ret==SUCCESS);
    
    ret = FlsFree(11);
    printf("Tried freeing index 11, result %d\n", ret==SUCCESS);
     
    ret = FlsFree(10);
    printf("Tried freeing index 10, result %d\n", ret==SUCCESS);
    
    ret = FlsFree(12);
    printf("Tried freeing index 11, result %d\n", ret==SUCCESS);
    
    ret = FlsFree(40);
    printf("Tried freeing index 40, result %d\n", ret==SUCCESS);
    
    ret = FlsFree(8);
    printf("Tried freeing index 40, result %d\n", ret==SUCCESS);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
}

int flsAllocSetGetFree(){
    
    long index;
    int ret;
    long long ret_val;
    long long write_value = 200000;
    
    index = FlsAlloc();
    printf("Tried FlsAlloc, got back index %ld\n", index);
    
    ret = FlsSetValue(index, write_value);
    printf("Tried FlsSetValue index %ld value %lld. Got result %d\n", index, write_value, ret==SUCCESS);
    
    ret_val = FlsGetValue(index);
    printf("Tried FlsGetValue index %ld. Got value %lld\n", index, ret_val);
    
    ret = FlsFree(index);
    printf("Tried freeing index %ld, result %d\n", index, ret==SUCCESS);
    
    return ret;
}

int flsAlloc_Until_err(){
    int successes = 0;
    int to_alloc=4096;
    long addr;
    do{
        addr = FlsAlloc();
        if(addr!=ERROR){
            successes++;
        } else {
            break;
        }
    } while(successes < to_alloc);
    printf("\n\nflsAlloc_Until_err - Tried allocating %d slots, got %d successes\n\n", to_alloc, successes);
    return 0;
}
