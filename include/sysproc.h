
#ifndef __SYSPROC_H__
#define __SYSPROC_H__


uint64_t syscall_argraw(int n);
void syscall_argint(int n, int *ip);
int  syscall_argaddr(int n, uint64_t *ip);
int syscall_argstr(int n, char *buf, int max);
int syscall_fetchstr(uint64 addr, char *buf, int max);

uint64_t sys_fork(void);
uint64_t sys_exit(void);
uint64_t sys_exec(void);
uint64_t sys_getpid(void);
uint64_t sys_debug(void);
uint64_t sys_mmap(void);
uint64_t sys_sbrk(void);
uint64_t sys_get_msg(void);
uint64_t sys_snd_msg(void);
uint64_t sys_rcv_msg(void);
uint64_t sys_get_shm(void);
uint64_t sys_att_shm(void);
uint64_t sys_irq_set(void);
uint64_t sys_irq_act(void);
uint64_t sys_act_sig(void);
uint64_t sys_snd_sig(void);
uint64_t sys_ipc_snd(void);
uint64_t sys_ipc_rcv(void);
uint64_t sys_ipc_rpl(void);
uint64_t sys_ipc_cll(void);
uint64_t sys_endpt_creat(void);
uint64_t sys_cap_grnt(void);
uint64_t __sys_cap_transfer(void);
uint64_t sys_cap_create(void);
uint64_t __sys_shmem_create(void);
uint64_t __sys_shmem_attach(void);
uint64_t __sys_cap_free(void);
uint64_t sys_fast_call(void);

#endif /* __SYSPROC_H__ */