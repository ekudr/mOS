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

int sbi_debug_console_write(const char *bytes, unsigned int num_bytes);

uint64_t sys_fork(void)
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
    pagetable_t     pgtable;
    task_t          *t = mytask();

    addr    = syscall_argraw(0);
    size     = syscall_argraw(1);
    flags   = syscall_argraw(2);
    paddr   = syscall_argraw(3);
    
    mem_reg_t * mreg = uvm_user_memmap(t, addr, size, flags, paddr);
//        debug("\x1b[35mPMPCFG0:\x1b[0m 0x%lX\n", r_pmpcfg0());
    return mreg->addr;
}

uint64_t sys_sbrk(void)
{
    uint64_t    addr;
    task_t      *t;
    int         size;

    syscall_argint(0, &size);
    t = mytask();
    if (size == 0)
        return t->mm->brk;

    if (size > 0)
    {
        addr = t->mm->brk;
        size = PGROUNDUP(size);
        if (uvm_alloc_mmreg(t, addr, size, MM_REG_MEM, PTE_U | PTE_R | PTE_W) != SUCCESS)        
            return -ENOMEM;
        
        t->mm->brk += size;
    }
    else if (size < 0)
    {
        panic("[SYSCALL] SBRK redues memory is not implemented yet");
    }

    
    return addr;
}

uint64_t sys_get_msg(void)
{
    uint64_t qkey, flags;
    qkey = syscall_argraw(0);
    flags = syscall_argraw(1);
    return ipc_get_msg(qkey, flags);
}

uint64_t sys_snd_msg(void)
{
    uint64_t qid, type, uaddr, size, flags;
    qid = syscall_argraw(0);
    type = syscall_argraw(1);
    uaddr = syscall_argraw(2);
    size = syscall_argraw(3);
    flags = syscall_argraw(4);
    return ipc_snd_msg(qid, type, uaddr, size, flags);
}

uint64_t sys_rcv_msg(void)
{
    uint64_t qid, type, uaddr, size, flags;
    qid = syscall_argraw(0);
    type = syscall_argraw(1);
    uaddr = syscall_argraw(2);
    size = syscall_argraw(3);
    flags = syscall_argraw(4);
    return ipc_rcv_msg(qid, type, uaddr, size, flags);
}

uint64_t sys_get_shm(void)
{
    uint64_t qkey, size, flags;
    qkey = syscall_argraw(0);
    size = syscall_argraw(1);
    flags = syscall_argraw(2);
    return (uint64_t)ipc_get_shm(qkey, size, flags);
}

uint64_t sys_att_shm(void)
{
    uint64_t shmid, addr, flags;
    shmid = syscall_argraw(0);
    addr = syscall_argraw(1);
    flags = syscall_argraw(2);
    return (uint64_t)ipc_att_shm(shmid, (const void *)addr, flags);
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
