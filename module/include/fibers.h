#include "common.h"

pid_t kernelConvertThreadToFiber    (pid_t tgid, \
                                    pid_t pid);  \

pid_t kernelCreateFiber             (long user_fn,      \
                                    void *param,        \
                                    pid_t tgid,         \
                                    pid_t pid,          \
                                    void *stack_base,   \
                                    size_t stack_size); 

pid_t kernelSwitchToFiber           (pid_t tgid, \
                                    pid_t pid,   \
                                    pid_t fid);


long kernelFlsAlloc                 (pid_t tgid, \
                                    pid_t pid);

int kernelFlsFree                   (pid_t tgid,          \
                                    pid_t pid,            \
                                    long index);

long long kernelFlsGetValue         (pid_t tgid,          \
                                    pid_t pid,            \
                                    long index);

int kernelFlsSetValue               (pid_t tgid,          \
                                    pid_t pid,            \
                                    long index,           \
                                    long long value);

void kernelProcCleanup (pid_t tgid); 
void kernelModCleanup  (void);
