#include "fibers_iface.h"
#include <stdio.h>

int main(){ 
    ConvertThreadToFiber();
    CreateFiber(NULL,NULL);
    SwitchToFiber(0);
}
