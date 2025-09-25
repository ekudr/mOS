#include <mosstd.h>
#include <syscall.h>
#include <errno.h>
#include <libsys/syscall.h>

kerrno_t ipc_send(uint32_t id, void *msg, uint64_t size)
{
    return __syscall(SYS_ipc_snd, (uint64_t)id, (uint64_t)msg, (uint64_t)size);
}

int ipc_receive(uint32_t id, void *msg, uint64_t size)
{
    return __syscall(SYS_ipc_rcv, (uint64_t)id, (uint64_t)msg, (uint64_t)size);
}

int ipc_endpoint_create(uint32_t rights)
{
    return __syscall(SYS_endpt_crt, (uint64_t) rights);
}

long cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights)
{
    return (long)__syscall(SYS_cap_garnt, (uint64_t)from_id, to_pid, (uint64_t)to_slot, (uint64_t)req_rights);
}

kerrno_t ipc_replay(uint32_t id, void *msg, uint64_t size)
{
    return __syscall(SYS_ipc_rpl, (uint64_t)id, (uint64_t)msg, size);
}

kerrno_t ipc_call(uint32_t id, void *msg, void *rep, uint64_t size)
{
    return __syscall(SYS_ipc_call, (uint64_t)id, (uint64_t)msg, (uint64_t)rep, (uint64_t)size);
}
