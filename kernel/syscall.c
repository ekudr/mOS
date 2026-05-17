#include <common.h>
#include <sched.h>
#include <syscall.h>
#include <sysproc.h>
#include <string.h>
#include<trap.h>

/*
 * Fetch the nul-terminated string at addr from the current process.
 * Returns length of string, not including nul, or -1 for error.
*/
int
syscall_fetchstr(uint64 addr, char *buf, int max)
{
    struct task *t = mytask();
    if (mmu_user_copyinstr(t->pagetable, buf, addr, max) < 0)
        return -1;
    return strlen(buf);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
syscall_argstr(int n, char *buf, int max)
{
    uint64_t addr;
    syscall_argaddr(n, &addr);
    return syscall_fetchstr(addr, buf, max);
}

uint64_t
syscall_argraw(int n)
{
    struct task *t = mytask();
    switch (n)
    {
    case 0:
        return t->trapframe->a0;
    case 1:
        return t->trapframe->a1;
    case 2:
        return t->trapframe->a2;
    case 3:
        return t->trapframe->a3;
    case 4:
        return t->trapframe->a4;
    case 5:
        return t->trapframe->a5;
    case 6:
        return t->trapframe->a6;
    }
    panic("argraw");
    return -1;
}

uint64_t syscall_get_MR(struct task *t, int n)
{
    switch (n)
    {
    case 0:
        return t->trapframe->a0;
    case 1:
        return t->trapframe->a1;
    case 2:
        return t->trapframe->a2;
    case 3:
        return t->trapframe->a3;
    case 4:
        return t->trapframe->a4;
    case 5:
        return t->trapframe->a5;
    case 6:
        return t->trapframe->a6;
    }
    panic("get mr");
    return -EINVAL;   
}

void syscall_set_MR(struct task *t, int n, uint64_t val)
{
    switch (n)
    {
    case 0:
        t->trapframe->a0 = val;
        break;
    case 1:
        t->trapframe->a1 = val;
        break;
    case 2:
        t->trapframe->a2 = val;
        break;
    case 3:
        t->trapframe->a3 = val;
        break;
    case 4:
        t->trapframe->a4 = val;
        break;
    case 5:
        t->trapframe->a5 = val;
        break;
    case 6:
        t->trapframe->a6 = val;
        break;
    default:
        panic("set mr");
    }
}

// Fetch the Nth 64-bit system call argument.
void
syscall_argint(int n, int *ip)
{
  *ip = syscall_argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
syscall_argaddr(int n, uint64_t *ip)
{
  *ip = syscall_argraw(n);
  return 0;
}

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64_t (*syscalls[])(void) = {
    [SYS_yield]     = __sys_yield,
    [SYS_wait]  = __sys_wait_irq,
//    [SYS_fork]      =  sys_fork,
    [SYS_exit]      =  sys_exit,
//    [SYS_exec]      =  sys_exec,
    [SYS_getpid]    =  sys_getpid,
    [SYS_debug]     =  sys_debug,
    [SYS_mmap]      =  sys_mmap,
    [SYS_cache_flush] = __sys_cache_flush,
    [SYS_cache_inval] = __sys_cache_inval,
//    [SYS_sbrk]      =  sys_sbrk,
    [SYS_recv]      =  __sys_recv,
    [SYS_nb_recv]   =  __sys_nb_recv,
    [SYS_send]      =  __sys_send,
    [SYS_nb_send]   =  __sys_nb_send,
    [SYS_call]      =  __sys_call,
    [SYS_reply]    =  __sys_reply,
    [SYS_irq_set]  =  sys_irq_set,
    [SYS_irq_act]  =  sys_irq_act,
    [SYS_sig_act]   = sys_act_sig,    
    [SYS_sig_snd]   = sys_snd_sig,
    [SYS_sig_ret]   = usersigret,
    // [SYS_ipc_snd]   = sys_ipc_snd,
    // [SYS_ipc_rcv]   = sys_ipc_rcv,
    // [SYS_ipc_rpl]   = sys_ipc_rpl,
    // [SYS_ipc_call]   = sys_ipc_cll,
    // [SYS_endpt_crt] = sys_endpt_creat,
    [SYS_cap_grant] = sys_cap_grnt,
//    [SYS_cap_transfer] = __sys_cap_transfer,
    [SYS_cap_crt]   = sys_cap_create,
    [SYS_shmem_crt] = __sys_shmem_create,
    [SYS_shmem_att] = __sys_shmem_attach,
//    [SYS_fast_call] = sys_fast_call,
    [SYS_cap_free]  = __sys_cap_free,
    [SYS_task_ctrl] = __sys_task_control,

    [SYS_notif_create]   = __sys_notif_create,
    [SYS_signal]         = __sys_signal,
    [SYS_notif_bind]     = __sys_notif_bind,
    [SYS_notif_unbind]   = __sys_notif_unbind,
    [SYS_irq_bind_notif] = __sys_irq_bind_notif,

};

void
syscall(void)
{
    int num;
    struct task *t = mytask();

    num = t->trapframe->a7;

      if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in p->trapframe->a0

        t->trapframe->a0 = syscalls[num]();
//        debug("[SYSCALL] return a0 = 0x%lX into task\n", t->trapframe->a0);
      } else {
        printf("task %d %s: unknown sys call %d\n",
                t->pid, t->name, num);
        t->trapframe->a0 = -1;
      }


//    debug("[SYSCALL] syscall %d from task %d\n", num, t->pid);
}