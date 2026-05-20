#include <stdint.h>
#include <libsys/cap.h>
#include <syscall.h>
#include <libsys/syscall.h>
#include <libsys/ipc.h>
#include <libsys/cap.h>

/*
 *  Create capability
 */ 
int create_capability(cap_type_t type, cap_rights_t rights)
{
    ipc_setMR(0, type);
    ipc_setMR(1, rights);
    
    uint64_t info = msginfo_word_new(0, 2, 0, 0);
    cap_create((uint64_t)-1, info);

    return ipc_getMR(0);
}

int cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights, uint64_t badge)
{
    return (int)__syscall(SYS_cap_grant, (uint64_t)from_id, to_pid, (uint64_t)to_slot, (uint64_t)req_rights, badge);
}

// int cap_transfer(int src_cap, int dest_cap, uint32_t req_rights)
// {
//     return (long)__syscall(SYS_cap_transfer, (uint64_t)src_cap, (uint64_t)dest_cap, (uint64_t)req_rights);
// }

// int cap_shmem_create_(uint64_t size, cap_rights_t rights)
// {

//     return __syscall(SYS_shmem_crt, (uint64_t)size, (uint64_t)rights);
// }

int cap_free(int cap_id)
{
    return __syscall(SYS_cap_free, (uint64_t)cap_id);
}

int cap_shmem_create(uint64_t size, cap_rights_t rights)
{
    ipc_setMR(0, CAP_SHMEMORY);
    ipc_setMR(1, rights);
    ipc_setMR(2, size);

    uint64_t info = msginfo_word_new(0, 3, 0, 0);
    cap_create((uint64_t)-1, info);

    return ipc_getMR(0);
}

int cap_frame_create(void *addr, uint64_t size, cap_rights_t rights)
{
    ipc_setMR(0, CAP_FRAME);
    ipc_setMR(1, rights);
    ipc_setMR(2, size);
    ipc_setMR(3, (uint64_t)addr);

    uint64_t info = msginfo_word_new(0, 4, 0, 0);
    cap_create((uint64_t)-1, info);

    return ipc_getMR(0);
}

int cap_dmamem_create(uint64_t size, cap_rights_t rights, uint64_t *paddr)
{
    ipc_setMR(0, CAP_DMA_FRAME);
    ipc_setMR(1, rights);
    ipc_setMR(2, size);

    uint64_t info = msginfo_word_new(0, 3, 0, 0);
    cap_create((uint64_t)-1, info);

    *paddr = ipc_getMR(1); 

    return ipc_getMR(0);
}

// All cap_task_* helpers below use __syscall so the kernel return value in a0
// is propagated to the caller. The kernel handler reads positional args from
// a0 (cap_id), a2 (op selector), a3+ (op-specific args) — same layout as
// syscall_send but without the IPC info word semantics.

int cap_task_mem_move(int cap_id, void *vaddr, int mem_cap, uint64_t flags)
{
    return (int)__syscall(SYS_task_ctrl, (uint64_t)cap_id, (uint64_t)0,
                          (uint64_t)SYS_TASK_OP_MEM_MAP, (uint64_t)vaddr,
                          (uint64_t)mem_cap, flags, (uint64_t)0);
}

int cap_task_run(int cap_id, uint64_t entry)
{
    return (int)__syscall(SYS_task_ctrl, (uint64_t)cap_id, (uint64_t)0,
                          (uint64_t)SYS_TASK_OP_RUN, entry, (uint64_t)0,
                          (uint64_t)0, (uint64_t)0);
}

int cap_task_getpid(int cap_id)
{
    return (int)__syscall(SYS_task_ctrl, (uint64_t)cap_id, (uint64_t)0,
                          (uint64_t)SYS_TASK_OP_GETPID, (uint64_t)0, (uint64_t)0,
                          (uint64_t)0, (uint64_t)0);
}

int cap_task_mem_share(int cap_id)
{
    return (int)__syscall(SYS_task_ctrl, (uint64_t)cap_id, (uint64_t)0,
                          (uint64_t)SYS_TASK_OP_MEM_SHARE, (uint64_t)0, (uint64_t)0,
                          (uint64_t)0, (uint64_t)0);
}

void *cap_shmem_attach(int shmem_cap, void *addr, int flags)
{
    return (void *)__syscall(SYS_shmem_att, (uint64_t)shmem_cap,
                             (uint64_t)addr, (uint64_t)flags);
}