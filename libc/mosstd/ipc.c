#include <mosstd.h>
#include <syscall.h>
#include <errno.h>
#include <libsys/syscall.h>

kerrno_t ipc_send(int id, void *msg, uint64_t size)
{
    return __syscall(SYS_ipc_snd, (uint64_t)id, (uint64_t)msg, (uint64_t)size);
}


/*
 *  Syscall IPC receive
 *  cap_id - capability id of in sender table
 *  uadd - message address in userland
 *  size - size of message
 *  flags - flags (IPC_NOWAIT)
 *  returns reply cap id
 */
int ipc_receive(int cap_id, void *msg, uint64_t size, int flags)
{
    return __syscall(SYS_ipc_rcv, (uint64_t)cap_id, (uint64_t)msg, (uint64_t)size, (uint64_t)flags);
}

int ipc_endpoint_create(uint32_t rights)
{
    return __syscall(SYS_endpt_crt, (uint64_t) rights);
}


kerrno_t ipc_reply(int cap_id, void *msg, uint64_t size)
{
    return __syscall(SYS_ipc_rpl, (uint64_t)cap_id, (uint64_t)msg, size);
}

kerrno_t ipc_call(int cap_id, void *msg, void *rep, uint64_t size)
{
    return __syscall(SYS_ipc_call, (uint64_t)cap_id, (uint64_t)msg, (uint64_t)rep, (uint64_t)size);
}

void *ipc_shm_attach(int cap_id, const void *addr, int flags)
{
    return (void *)__syscall(SYS_shmem_att, (uint64_t)cap_id, (uint64_t)addr, (uint64_t)flags);
}