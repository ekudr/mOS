#include <stdint.h>
#include <stddef.h>
#include <syscall.h>
#include <libsys/cap.h>
#include <libsys/ipc.h>
#include <libsys/thread.h>

__attribute__((noreturn)) void exit(int status);

// Layout of the shared region between parent and child.
// Parent writes; child reads after attaching the region.
typedef struct {
    uintptr_t fn;   // void (*)(void *) cast to uintptr_t
    uintptr_t arg;
} thread_shm_t;

// Called from thread_entry.S after __ipc_buffer and GP are initialized.
void __thread_main(void)
{
    // Attach the shmem cap the parent granted at the well-known slot.
    thread_shm_t *shm = (thread_shm_t *)cap_shmem_attach(NS_THREAD_SHM_SLOT, NULL, 0);
    if (!shm)
        exit(1);

    void (*fn)(void *) = (void (*)(void *))shm->fn;
    void *arg          = (void *)shm->arg;

    fn(arg);
    exit(0);
}

int thread_spawn(void (*entry)(void *), void *arg,
                 size_t shared_size, thread_handle_t *out)
{
    int task_cap, shmem_cap, child_pid, ret;
    thread_shm_t *shared;

    if (!entry || !out)
        return -1;

    // 1. Create child task; installs a CAP_TASK in caller's cspace.
    task_cap = create_capability(CAP_TASK, 0);
    if (task_cap < 0)
        return task_cap;

    // 2. Share caller's MM_REG_MEM regions (code, rodata, data, bss) into the
    //    child at matching VAs. Without this the child page-faults the moment
    //    the scheduler jumps to __thread_trampoline — that address only exists
    //    in the parent's page table otherwise.
    ret = cap_task_mem_share(task_cap);
    if (ret < 0) {
        cap_free(task_cap);
        return ret;
    }

    // 3. Get child's PID so we can grant caps into its cspace.
    child_pid = cap_task_getpid(task_cap);
    if (child_pid < 0) {
        cap_free(task_cap);
        return child_pid;
    }

    // 4. Create shmem region; needs CRIGHT_GRANT so we can grant it to child.
    shmem_cap = cap_shmem_create(shared_size, CRIGHT_MAP | CRIGHT_GRANT);
    if (shmem_cap < 0) {
        cap_free(task_cap);
        return shmem_cap;
    }

    // 5. Attach shmem in parent's address space.
    shared = (thread_shm_t *)cap_shmem_attach(shmem_cap, NULL, 0);
    if (!shared) {
        cap_free(shmem_cap);
        cap_free(task_cap);
        return -1;
    }

    // 6. Write fn and arg so the child trampoline can read them after attach.
    shared->fn  = (uintptr_t)entry;
    shared->arg = (uintptr_t)arg;

    // 7. Grant shmem cap into child's cspace at NS_THREAD_SHM_SLOT.
    ret = cap_grant(shmem_cap, (uint64_t)child_pid,
                    NS_THREAD_SHM_SLOT, CRIGHT_MAP, 0);
    if (ret < 0) {
        cap_free(shmem_cap);
        cap_free(task_cap);
        return ret;
    }

    // 8. Mark child RUNNABLE; it starts executing at __thread_trampoline.
    ret = cap_task_run(task_cap, (uint64_t)__thread_trampoline);
    if (ret < 0) {
        cap_free(shmem_cap);
        cap_free(task_cap);
        return ret;
    }

    out->task_cap    = task_cap;
    out->shmem_cap   = shmem_cap;
    out->shared_buf  = shared;
    out->shared_size = shared_size;
    return 0;
}
