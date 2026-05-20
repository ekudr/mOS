#ifndef __LIBSYS_THREAD_H__
#define __LIBSYS_THREAD_H__

#include <stdint.h>
#include <stddef.h>

// Well-known cap slot in child's cspace where the shmem cap is granted.
// cap_task_create installs frame caps at low slots (stack + ipc_buf MEMIO), so
// 0x200 / 0x300 are in use by the time thread_spawn runs. Use a slot well
// above any default-installed caps to avoid -EEXIST from cap_grant.
#define NS_THREAD_SHM_SLOT  0x1000

typedef struct {
    int    task_cap;     // CAP_TASK in caller's cspace
    int    shmem_cap;    // CAP_SHMEMORY cap in caller's cspace
    void  *shared_buf;   // shmem region VA in caller's address space
    size_t shared_size;
} thread_handle_t;

// Spawn a child task running entry(arg) using existing kernel cap primitives.
// The child shares a shmem region with the parent; fn and arg are passed
// through it. The child is NOT a true pthread — it has its own address space
// and only shares what is explicitly attached via shmem.
//   entry      : user function (void (*)(void *)) to run in the child
//   arg        : opaque argument passed to entry
//   shared_size: size of shmem region in bytes (rounded up to PAGE_SIZE)
//   out        : populated on success; caller owns task_cap and shmem_cap
// Returns 0 on success, negative errno on failure.
int thread_spawn(void (*entry)(void *), void *arg,
                 size_t shared_size, thread_handle_t *out);

// Child-side trampoline: attaches the shmem cap at NS_THREAD_SHM_SLOT,
// reads fn+arg from the region, and dispatches. Never returns.
void __thread_trampoline(void);

#endif /* __LIBSYS_THREAD_H__ */
