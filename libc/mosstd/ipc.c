#include <mosstd.h>
#include <syscall.h>
#include <errno.h>
#include <libsys/syscall.h>
#include <libsys/ipc.h>
#include <ipc.h>



int ipc_endpoint_create(uint32_t rights)
{
    return __syscall(SYS_endpt_crt, (uint64_t) rights);
}


void *ipc_shm_attach(int cap_id, const void *addr, int flags)
{
    return (void *)__syscall(SYS_shmem_att, (uint64_t)cap_id, (uint64_t)addr, (uint64_t)flags);
}
