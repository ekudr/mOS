#include <mosstd.h>
#include <signals.h>
#include <syscall.h>
#include <libsys/syscall.h>

kerrno_t signal_send(pid_t pid, signal_t sig, uint64_t payload)
{
    return __syscall(SYS_sig_snd, (uint64_t)pid, (uint64_t)sig, payload);
}
