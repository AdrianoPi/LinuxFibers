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


// Allocates one Fiber Local Storage entry
long FlsAlloc();

// Frees a Fiber Local Storage entry
// @index: index identifier of the entry to be freed
int FlsFree(long index);

// Gets value of a Fiber Local Storage entry
// @index: index identifier of the entry to be read
long long FlsGetValue(long index);

// Sets value of a Fiber Local Storage entry
// @index: index identifier of the entry to be written
// @value: value to be written
int FlsSetValue(long index, long long value);

int FiberExit();
