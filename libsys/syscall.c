#include <common.h>
#include <syscall.h>
#include <libsys/syscall.h>


__attribute__((noreturn)) void exit(int status)
{
    for(;;)
    {
        __syscall1(SYS_exit, status);
    }
    
}
/*
uint64_t getpid(void)
{
    return __syscall(SYS_getpid);
}
*/

int snd_msg(uint64_t qid, uint64_t type, uintptr_t buf, uint64_t size, uint64_t flags)
{
    return __syscall(SYS_snd_msg, qid, type, buf, size, flags);
}

int rcv_msg(uint64_t qid, uint64_t type, uintptr_t buf, uint64_t size, uint64_t flags)
{
    return __syscall(SYS_rcv_msg, qid, type, buf, size, flags);
}

uint64_t get_msg(uint64_t qkey, uint64_t flags)
{
    return __syscall(SYS_get_msg, qkey, flags);
}

int sys_debug(char *msg)
{
    return __syscall(SYS_debug, (uint64_t)msg);
}

char* sbrk(int size)
{
    return (char *) __syscall(SYS_sbrk, (uint64_t)size);
}

void *mmap(void *addr, uint64_t len, uint64_t flags, void *paddr)
{
    return (void *)__syscall(SYS_mmap,  (uint64_t)addr,(uint64_t)len, (uint64_t)flags, (uint64_t)paddr);
}

uint64_t shmget(uint64_t key, size_t size, uint64_t shmflg)
{
    return (uint64_t)__syscall(SYS_shm_get, (uint64_t)key, (uint64_t)size, (uint64_t)shmflg);
}

void *shmat(int shmid, const void *shmaddr, uint64_t shmflg)
{
    return (void *)__syscall(SYS_shm_att, (uint64_t)shmid, (uint64_t)shmaddr, (uint64_t)shmaddr);
}

int shmdt(const void *shmaddr)
{
    return (int)__syscall(SYS_shm_det, (uint64_t)shmaddr);
}



kerrno_t irq_set(uint64_t irq, uint64_t flags)
{
    return __syscall(SYS_irq_set, (uint64_t) irq, (uint64_t) flags);
}

kerrno_t irq_act(uint64_t irq, uint64_t flags)
{
    return __syscall(SYS_irq_act, (uint64_t) irq, (uint64_t) flags);
}