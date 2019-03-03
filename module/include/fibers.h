#include "common.h"

pid_t kernelConvertThreadToFiber  (pid_t tgid,pid_t pid);
pid_t kernelCreateFiber           (long user_fn, void *param, pid_t tgid,pid_t pid, void *stack_base, size_t stack_size);
pid_t kernelSwitchToFiber         (pid_t tgid, pid_t pid, pid_t fid);
