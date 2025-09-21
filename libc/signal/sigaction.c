#include <stddef.h>
#include <signals.h>
#include <errno.h>
#include <syscall.h>
#include <libsys/syscall.h>

kerrno_t signal_action(signal_t sig, signal_action_t *sa)
{
    if(sa == NULL)
        return -EINVAL;

    sa->restorer = __restore;
    return __syscall(SYS_sig_act, (uint64_t)sig, (uint64_t)sa);
}

