#include <common.h>
#include <syscall.h>
#include <libsys/syscall.h>
#include <libsys/ipc.h>


__attribute__((noreturn)) void exit(int status)
{
    for(;;)
    {
        __syscall1(SYS_exit, status);
    }
    
}


// int snd_msg(uint64_t qid, uint64_t type, uintptr_t buf, uint64_t size, uint64_t flags)
// {
//     return __syscall(SYS_snd_msg, qid, type, buf, size, flags);
// }

// int rcv_msg(uint64_t qid, uint64_t type, uintptr_t buf, uint64_t size, uint64_t flags)
// {
//     return __syscall(SYS_rcv_msg, qid, type, buf, size, flags);
// }

// uint64_t get_msg(uint64_t qkey, uint64_t flags)
// {
//     return __syscall(SYS_get_msg, qkey, flags);
// }

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

int cash_flash(void *addr, size_t size)
{
    return (int) __syscall(SYS_cache_flush, (uint64_t)addr, (uint64_t)size);
}

kerrno_t irq_set(uint64_t irq, uint64_t flags)
{
    return __syscall(SYS_irq_set, (uint64_t) irq, (uint64_t) flags);
}

kerrno_t irq_act(uint64_t irq, uint64_t flags)
{
    return __syscall(SYS_irq_act, (uint64_t) irq, (uint64_t) flags);
}

uint64_t ipc_recv(uint64_t src, uint64_t *sender)
{
    msg_info_t info;
    uint64_t badge;
    uint64_t msg0;
    uint64_t msg1;
    uint64_t msg2;
    uint64_t msg3;
    uint64_t msg4;

    syscall_recv(SYS_recv, src, &badge, &info, &msg0, &msg1, &msg2, &msg3, &msg4);

    ipc_setMR(0, msg0);
    ipc_setMR(1, msg1);
    ipc_setMR(2, msg2);
    ipc_setMR(3, msg3);
    ipc_setMR(4, msg4);

    /* Return back sender and message information. */
    if (sender) {
        *sender = badge;
    }
    return info;
}

uint64_t ipc_nb_recv(uint64_t src, uint64_t *sender)
{
    msg_info_t info;
    uint64_t badge;
    uint64_t msg0;
    uint64_t msg1;
    uint64_t msg2;
    uint64_t msg3;
    uint64_t msg4;

    syscall_recv(SYS_nb_recv, src, &badge, &info, &msg0, &msg1, &msg2, &msg3, &msg4);

    ipc_setMR(0, msg0);
    ipc_setMR(1, msg1);
    ipc_setMR(2, msg2);
    ipc_setMR(3, msg3);
    ipc_setMR(4, msg4);

    /* Return back sender and message information. */
    if (sender) {
        *sender = badge;
    }
    return info;
}

void ipc_send(uint64_t dest, msg_info_t info)
{
    syscall_send(SYS_send, dest, info, ipc_getMR(0), ipc_getMR(1),
                   ipc_getMR(2), ipc_getMR(3), ipc_getMR(4));
}

void ipc_nb_send(uint64_t dest, msg_info_t info)
{
    syscall_send(SYS_nb_send, dest, info, ipc_getMR(0), ipc_getMR(1),
                   ipc_getMR(2), ipc_getMR(3), ipc_getMR(4));
}

uint64_t  ipc_call(uint64_t dest, msg_info_t info)
{
    msg_info_t o_info;
    uint64_t msg0 = ipc_getMR(0);
    uint64_t msg1 = ipc_getMR(1);
    uint64_t msg2 = ipc_getMR(2);
    uint64_t msg3 = ipc_getMR(3);
    uint64_t msg4 = ipc_getMR(4);

    syscall_send_recv(SYS_call, dest, &dest, info, &o_info, &msg0, &msg1,
                        &msg2, &msg3, &msg4);

    /* Write out the data back to memory. */
    ipc_setMR(0, msg0);
    ipc_setMR(1, msg1);
    ipc_setMR(2, msg2);
    ipc_setMR(3, msg3);
    ipc_setMR(4, msg4);

    return o_info;
}

void ipc_reply(msg_info_t info)
{
    syscall_reply(SYS_reply, info, ipc_getMR(0), ipc_getMR(1), ipc_getMR(2),
                    ipc_getMR(3), ipc_getMR(4));
}

int cap_create(uint64_t dest, msg_info_t info)
{
    msg_info_t o_info;
    uint64_t msg0 = ipc_getMR(0);
    uint64_t msg1 = ipc_getMR(1);
    uint64_t msg2 = ipc_getMR(2);
    uint64_t msg3 = ipc_getMR(3);
    uint64_t msg4 = ipc_getMR(4);

    syscall_send_recv(SYS_cap_crt, dest, &dest, info, &o_info, &msg0, &msg1,
                        &msg2, &msg3, &msg4);

    /* Write out the data back to memory. */
    ipc_setMR(0, msg0);
    ipc_setMR(1, msg1);
    ipc_setMR(2, msg2);
    ipc_setMR(3, msg3);
    ipc_setMR(4, msg4);

    return o_info;
}

