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

int cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights)
{
    return (long)__syscall(SYS_cap_grant, (uint64_t)from_id, to_pid, (uint64_t)to_slot, (uint64_t)req_rights);
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

int cap_task_mem_move(int cap_id, void *vaddr, int mem_cap, uint64_t flags)
{
    // TASK_OP_MEM_MOVE
    syscall_send(SYS_task_ctrl, cap_id, 0, 2, (uint64_t)vaddr,
                   (uint64_t)mem_cap, flags, 0);

    return 0;
}

int cap_task_run(int cap_id, uint64_t entry)
{
    // TASK_OP_RUN
    syscall_send(SYS_task_ctrl, cap_id, 0, 3, entry, 0, 0, 0);

    return 0;
}