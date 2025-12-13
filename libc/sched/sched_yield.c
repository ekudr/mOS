#include <syscall.h>
#include <libsys/syscall.h>


int sched_yield(void)
{
    return __syscall(SYS_yield);
}