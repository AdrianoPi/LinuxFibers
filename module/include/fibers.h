#include "common.h"

pid_t kernelConvertThreadToFiber  (pid_t tid);
pid_t kernelCreateFiber           (void (*user_fn)(void *), void *param, pid_t tid);
pid_t kernelSwitchToFiber         (pid_t fid, pid_t tid);
