#include <sys/types.h>
#include <stdio.h>

pid_t ConvertThreadToFiber();

pid_t CreateFiber(void (*user_func)(void*), void *);
long SwitchToFiber(pid_t);
/*
long FlsAlloc();
bool FlsFree(long);
long long FlsGetValue(long);
void FlsSetValue(long long, long);
*/

