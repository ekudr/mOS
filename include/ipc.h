#ifndef __IPC_H__
#define __IPC_H__

#include <list.h>
#include <spinlock.h>
#include <sched.h>


#define IPC_NOWAIT  0x01     // do not sleep task (non-blocking ipc)
#define IPC_EXIST   0x02     // do not create new

struct ipc_manager
{
    spinlock_t lock;
    list_head_t qlist;
    // ??? it needs lock
    list_head_t shqlist;
};

typedef struct ipc_manager ipc_manager_t;

struct mqueue
{
    spinlock_t  lock;
    uint64_t    id;
    uint64_t    qkey;
    uint64_t    flags;
    list_head_t qlist;
    list_head_t mlist;
    task_t      *task;
};

typedef struct mqueue mqueue_t;

#define IPC_CALL 1
#define IPC_REPL 2

typedef struct ipc_msg
{
    uint64_t    type;
    list_head_t mlist;
    task_t      *sender;
    int         reply;
    char message[1];
}ipc_msg_t;

#define MAX_FC_ARS  6
typedef struct ipc_fastcall_msg
{
//    uint64_t    type;
    list_head_t mlist;
    task_t      *sender;
    uint64_t    arg[MAX_FC_ARS];
}ipc_fastcall_msg_t;

int ipc_init(void);
uint64_t ipc_get_msg(uint64_t qkey, uint64_t flags);
int ipc_snd_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags);
int ipc_rcv_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags);

uint64_t ipc_get_shm(uint64_t key, size_t size, uint64_t flags);
void *ipc_att_shm(uint64_t shmid, const void *addr, int flags);

void *sys_ipc_shm_attach(task_t *t, int cap_id, const void *addr, int flags);

uint64_t sys_ipc_call(task_t *t, uint32_t id, uint64_t umsg, uint64_t urep, uint64_t size);
uint64_t sys_ipc_recv(task_t *t, uint32_t id, uint64_t uaddr, uint64_t size, int flags);

uint64_t sys_ipc_send(task_t *t, uint32_t id, uint64_t uaddr, uint64_t size);
uint64_t sys_ipc_replay(task_t *t, uint64_t id, uint64_t uaddr, uint64_t size);

#endif /* __IPC_H__ */