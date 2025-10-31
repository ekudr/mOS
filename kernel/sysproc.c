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

    addr = t->mm->brk;
    if (size > 0)
    {        
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

uint64_t sys_ipc_snd(void)
{
    uint64_t id, uaddr, size;
    id = syscall_argraw(0);
    uaddr = syscall_argraw(1);
    size = syscall_argraw(2);
    task_t *t = mytask();
    return sys_ipc_send(t, id, uaddr, size);
}

uint64_t sys_ipc_rcv(void)
{
    uint64_t id, uaddr, size, flags;
    id = syscall_argraw(0);
    uaddr = syscall_argraw(1);
    size = syscall_argraw(2);
    flags = syscall_argraw(3);
    task_t *t = mytask();
    return sys_ipc_recv(t, id, uaddr, size, flags);
}

uint64_t sys_ipc_rpl(void)
{
    uint64_t id, uaddr, size;
    id    = syscall_argraw(0);
    uaddr = syscall_argraw(1);
    size  = syscall_argraw(2);
    task_t *t = mytask();
    return sys_ipc_replay(t, id, uaddr, size);
}

uint64_t sys_ipc_cll(void)
{
    uint64_t id, umsg, urep, size;
    id = syscall_argraw(0);
    umsg = syscall_argraw(1);
    urep = syscall_argraw(2);
    size = syscall_argraw(3);
    task_t *t = mytask();
    return sys_ipc_call(t, id, umsg, urep, size);
}

uint64_t sys_endpt_creat(void)
{
    uint64_t right;
    right = syscall_argraw(0);
    task_t *t = mytask();
    return sys_endpoint_create(t, right);
}

uint64_t sys_cap_grnt(void)
{
    uint64_t from_id, to_pid, to_slot, req_rights;
    from_id     = syscall_argraw(0);
    to_pid      = syscall_argraw(1);
    to_slot     = syscall_argraw(2); 
    req_rights  = syscall_argraw(3); 
    return sys_cap_grant((int)from_id, to_pid, (int)to_slot, (uint32_t)req_rights);
}

uint64_t __sys_cap_transfer(void)
{
    uint64_t dest_cap, src_cap, req_rights;
    src_cap     = syscall_argraw(0);
    dest_cap      = syscall_argraw(1);
    req_rights  = syscall_argraw(2); 
    return sys_cap_transfer(src_cap, dest_cap, req_rights);
}

uint64_t sys_cap_create(void)
{
    uint64_t type, rights;
    type = syscall_argraw(0);
    rights = syscall_argraw(1);
    task_t *t = mytask();
    return sys_capability_create(t, type, rights);
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
    uint64_t size, rights;
    size = syscall_argraw(0);
    rights = syscall_argraw(1);
    task_t *t = mytask();
    return sys_cap_shmem_create(t, size, rights);
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

uint64_t sys_fast_call(void)
{
    uint64_t arg1, arg2, arg3, arg4, arg5, arg6, arg7;
    arg1 = syscall_argraw(0);
    arg2 = syscall_argraw(1);
    arg3 = syscall_argraw(2);
    arg4 = syscall_argraw(3);
    arg5 = syscall_argraw(4);
    arg6 = syscall_argraw(5);
    arg7 = syscall_argraw(6);
    debug("[IPC_CALL] arg1 0x%lX arg2 0x%lX arg3 0x%lX arg4 0x%lX arg5 0x%lX arg6 0x%lX arg7 0x%lX\n",
                arg1, arg2, arg3, arg4, arg5, arg6, arg7);
        
    task_t *t = mytask();

    t->trapframe->a1 = 0x101;
    t->trapframe->a2 = 0x103;
    t->trapframe->a3 = 0x104;
    t->trapframe->a4 = 0x105;
    t->trapframe->a5 = 0x106;
    t->trapframe->a6 = 0x107;

    return 0x100;
}
