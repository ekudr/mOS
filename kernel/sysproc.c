#include <common.h>
#include <sysproc.h>
#include <memory.h>
#include <sched.h>
#include <ipc.h>
#include <mmu.h>
#include <riscv.h>
#include <signals.h>
#include <khash.h>
#include <irq.h>
#include <cap.h>

int sbi_debug_console_write(const char *bytes, unsigned int num_bytes);

inline void __sys_error_return(task_t *t, int err)
{
    uint32_t info = msginfo_word_new((uint32_t)err, 0, 0, 0);
    syscall_set_MR(t, 1, info);
}

uint64_t __sys_yield(void)
{
    sched_task_yield();
}

uint64_t sys_fork(void)
{
    return 0;
}

uint64_t __sys_wait_irq(void)
{
    return 0;
}
uint64_t sys_exit(void)
{
    int n;
    syscall_argint(0, &n);
    sched_task_exit(n);
    return 0;
}

uint64_t sys_exec(void)
{
    return 0;
}

uint64_t sys_getpid(void)
{
    return mytask()->pid;
}

uint64_t sys_debug(void)
{
    char *str;
    str = (char *)PPN2DA(pgalloc());
//    memset(str, 0, PAGE_SIZE);
    int c = syscall_argstr(0, str, 4096);
    if (c < 0){
        return -1;
    }
//    debug("%s", str);
    uint64_t ret = sbi_debug_console_write(str, c);
    pgfree(DA2PPN(str));
    return ret;
}

uint64_t sys_mmap(void)
{
    uint64_t        addr, paddr, size, flags;
    task_t          *t = mytask();

    addr    = syscall_argraw(0);
    size     = syscall_argraw(1);
    flags   = syscall_argraw(2);
    paddr   = syscall_argraw(3);
    
    mem_reg_t * mreg = uvm_user_memmap(t, addr, size, flags, paddr);
//        debug("\x1b[35mPMPCFG0:\x1b[0m 0x%lX\n", r_pmpcfg0());
    return mreg->addr;
}

// uint64_t sys_sbrk(void)
// {
//     uint64_t    addr;
//     task_t      *t;
//     int         size;

//     syscall_argint(0, &size);
//     t = mytask();
//     if (size == 0)
//         return t->mm->brk;

//     addr = t->mm->brk;
//     if (size > 0)
//     {        
//         size = PGROUNDUP(size);
//         if (uvm_alloc_mmreg(t, addr, size, MM_REG_MEM, PTE_U | PTE_R | PTE_W) != SUCCESS)        
//             return -ENOMEM;
        
//         t->mm->brk += size;
//     }
//     else if (size < 0)
//     {
//         panic("[SYSCALL] SBRK redues memory is not implemented yet");
//     }

    
//     return addr;
// }

void flush_dcache_range(unsigned long start, unsigned long end);

uint64_t __sys_cache_flush(void)
{
    uint64_t addr = syscall_argraw(0);
    uint64_t size = syscall_argraw(1);
    flush_dcache_range(addr, addr + size);

    return 0;
}

void invalidate_dcache_range(unsigned long start, unsigned long end);

uint64_t __sys_cache_inval(void)
{
    uint64_t addr = syscall_argraw(0);
    uint64_t size = syscall_argraw(1);
    invalidate_dcache_range(addr, addr + size);

    return 0;
}

uint64_t sys_irq_set(void)
{
    uint64_t irq, flags;
    irq       = syscall_argraw(0);
    flags     = syscall_argraw(1);
    task_t *t = mytask();
    return (uint64_t)irq_set(irq, t, flags);
}

uint64_t sys_irq_act(void)
{
    uint64_t irq, sa;
    irq       = syscall_argraw(0);
    sa        = syscall_argraw(1);
    task_t *t = mytask();
    return (uint64_t)irq_act(irq, t, sa);
}


uint64_t sys_act_sig(void)
{
    uint64_t sig, sa;

    sig = syscall_argraw(0);
    sa  = syscall_argraw(1);

    return (uint64_t)signal_action(sig, (uintptr_t) sa);
}

uint64_t sys_snd_sig(void)
{
    uint64_t tid, sig, payload;
    task_t *dst_task;

    tid      = syscall_argraw(0);
    sig      = syscall_argraw(1);
    payload  = syscall_argraw(2);  

    dst_task = sched_find_task(tid);

    return (uint64_t)signal_send(dst_task, sig, payload);
}


// uint64_t sys_endpt_creat(void)
// {
//     uint64_t right;
//     right = syscall_argraw(0);
//     task_t *t = mytask();
//     return sys_endpoint_create(t, right);
// }

uint64_t sys_cap_grnt(void)
{
    uint64_t from_id, to_pid, to_slot, req_rights;
    from_id     = syscall_argraw(0);
    to_pid      = syscall_argraw(1);
    to_slot     = syscall_argraw(2); 
    req_rights  = syscall_argraw(3); 
    return sys_cap_grant((int)from_id, to_pid, (int)to_slot, (uint32_t)req_rights);
}

// uint64_t __sys_cap_transfer(void)
// {
//     uint64_t dest_cap, src_cap, req_rights;
//     src_cap     = syscall_argraw(0);
//     dest_cap      = syscall_argraw(1);
//     req_rights  = syscall_argraw(2); 
//     return sys_cap_transfer(src_cap, dest_cap, req_rights);
// }

uint64_t sys_cap_create(void)
{
    // uint64_t type, rights;
    // type = syscall_argraw(2);
    // rights = syscall_argraw(3);
    task_t *t = mytask();
    int cap_id = sys_capability_create(t);
    t->trapframe->a2 = cap_id;
    return t->trapframe->a1;
}

uint64_t __sys_cap_free(void)
{
    uint64_t cap_id;
    cap_id = syscall_argraw(0);

    task_t *t = mytask();
    cap_free(t, cap_id);
    return 0;
}

uint64_t __sys_shmem_create(void)
{
    // uint64_t size, rights;
    // size = syscall_argraw(0);
    // rights = syscall_argraw(1);
    task_t *t = mytask();
    return sys_cap_shmem_create(t);
}

uint64_t __sys_shmem_attach(void)
{
    uint64_t cap_id, addr, flags;
    cap_id = syscall_argraw(0);
    addr = syscall_argraw(1);
    flags = syscall_argraw(2);
    task_t *t = mytask();
    return (uint64_t)sys_ipc_shm_attach(t, (int)cap_id, (const void *)addr, (int)flags);
}

uint64_t __sys_dmamem_attach(void)
{
    int ret;
    task_t *t = mytask();

    uint64_t cap_id = syscall_argraw(0);

    ret = sys_dmamem_attach(t, (int)cap_id);
    if (ret < 0) __sys_error_return(t, ret);
    return t->trapframe->a0;
}

uint64_t __sys_recv(void)
{
    int ret;
    task_t *t = mytask();

    uint64_t cap_id = syscall_argraw(0);

    ret = sys_ipc_recieve(t, cap_id, true);
    if (ret < 0) __sys_error_return(t, ret);

    // should return bange in a0
    // it's already in a0
    // ??? rewrite syscall func

    //debug("\x1b[31m0x%lX\x1b[0m", t->trapframe->a1);

    return t->trapframe->a0;
}

uint64_t __sys_nb_recv(void)
{
    int ret;
    task_t *t = mytask();

    uint64_t cap_id = syscall_argraw(0);

    ret = sys_ipc_recieve(t, cap_id, false);
    if (ret < 0) __sys_error_return(t, ret);

    // should return bange in a0
    // it's already in a0
    // ??? rewrite syscall func

    //debug("\x1b[31m0x%lX\x1b[0m", t->trapframe->a1);

    return t->trapframe->a0;
}

uint64_t __sys_send(void)
{
    int ret;
    task_t *t = mytask();

    uint64_t cap_id = syscall_argraw(0);

    ret = sys_ipc_send(t, cap_id, true, false);
    if (ret < 0) __sys_error_return(t, ret);

    // should return bange in a0
    // it's already in a0
    // ??? rewrite syscall func

    return t->trapframe->a0;
}

uint64_t __sys_nb_send(void)
{
    int ret;
    task_t *t = mytask();

    uint64_t cap_id = syscall_argraw(0);

    ret = sys_ipc_send(t, cap_id, false, false);
    if (ret < 0) __sys_error_return(t, ret);

    // should return bange in a0
    // it's already in a0
    // ??? rewrite syscall func

    return t->trapframe->a0;
}

uint64_t __sys_call(void)
{
    int ret;
    task_t *t = mytask();

    uint64_t cap_id = syscall_argraw(0);

    ret = sys_ipc_send(t, cap_id, true, true);
    if (ret < 0) __sys_error_return(t, ret);
    
    // should return bange in a0
    // it's already in a0
    // ??? rewrite syscall func

    return t->trapframe->a0;
}

uint64_t __sys_reply(void)
{
    int err;
    task_t *t = mytask();
//    debug("\x1b[31m[IPC]\x1b[0m reply task %d info 0x%lX\n", t->pid, t->trapframe->a1);
    err = sys_ipc_reply(t);
    if (err < 0) __sys_error_return(t, err);

    return t->trapframe->a0;
}

uint64_t __sys_task_control(void)
{
    int err;
    err = sched_task_control(mytask());

    return err;
}