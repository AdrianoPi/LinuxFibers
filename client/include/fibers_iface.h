#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

// Converts the current thread into a Fiber and allows from now on to 
// create other Fibers and switch among them.
pid_t ConvertThreadToFiber();

// Creates a new fiber that we can schedule from now on
// @user_func : function that the Fiber will start executing
// @user_param: the void* pointer to data that will be passed to user_func 
pid_t CreateFiber(void (*user_func)(void*), void *user_param );

// Changes the current context of execution into the one of a given Fiber
// @fiber_id: id of the Fiber that we want to schedule
pid_t SwitchToFiber(pid_t fiber_id);

/*
long FlsAlloc();
bool FlsFree(long);
long long FlsGetValue(long);
void FlsSetValue(long long, long);
*/

