#ifndef __IPC_H__
#define __IPC_H__

#include <list.h>
#include <spinlock.h>
#include <sched.h>


#define IPC_NOWAIT  0x100000000     // do not sleep task
#define IPC_EXIST   0x200000000     // do not create new

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



typedef struct ipc_msg
{
    uint64_t type;
    list_head_t mlist;
    char message[1];
}ipc_msg_t;


int ipc_init(void);
uint64_t ipc_get_msg(uint64_t qkey, uint64_t flags);
int ipc_snd_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags);
int ipc_rcv_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags);

uint64_t ipc_get_shm(uint64_t key, size_t size, uint64_t flags);
void *ipc_att_shm(uint64_t shmid, const void *addr, int flags);

#endif /* __IPC_H__ */