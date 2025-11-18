#ifndef __IPC_H__
#define __IPC_H__

#include <list.h>
#include <spinlock.h>


#include <sched.h>

struct task;

#define IPC_MAX_REGS  5
#define IPC_MAX_CAPS 4
#define IPC_MAX_MSG_LEN 500 

typedef struct {
    uint32_t    label;
    uint16_t    length;
    uint8_t     extra_caps;
    uint8_t     flags;
} ipc_msg_info_t;

#define MSGINFO_LABEL_BITS    32
#define MSGINFO_LABEL_SHIFT   32
#define MSGINFO_LABEL_MASK    ((1ULL << MSGINFO_LABEL_BITS)-1)

#define MSGINFO_LEN_BITS    16
#define MSGINFO_LEN_SHIFT   16
#define MSGINFO_LEN_MASK    ((1ULL << MSGINFO_LEN_BITS)-1)

#define MSGINFO_XCAPS_BITS    8
#define MSGINFO_XCAPS_SHIFT   8
#define MSGINFO_XCAPS_MASK    ((1ULL << MSGINFO_XCAPS_BITS)-1)

#define MSGINFO_FLAGS_BITS    8
#define MSGINFO_FLAGS_SHIFT   0
#define MSGINFO_FLAGS_MASK    ((1ULL << MSGINFO_FLAGS_BITS)-1)

inline uint64_t word_from_msginfo(ipc_msg_info_t *mi) 
{   
    uint64_t info = 0;
    info = (mi->label & MSGINFO_LABEL_MASK) << MSGINFO_LABEL_SHIFT; 
    info |= (mi->length & MSGINFO_LEN_MASK) << MSGINFO_LEN_SHIFT;
    info |= (mi->extra_caps & MSGINFO_XCAPS_MASK) << MSGINFO_XCAPS_SHIFT;
    info |= (mi->flags & MSGINFO_FLAGS_MASK);
    return info;
}

inline ipc_msg_info_t msginfo_from_word(uint64_t w) 
{   
    ipc_msg_info_t minfo;
    minfo.label = (w >> MSGINFO_LABEL_SHIFT) & MSGINFO_LABEL_MASK; 
    minfo.length = (w >> MSGINFO_LEN_SHIFT) & MSGINFO_LEN_MASK;
    minfo.extra_caps = (w >> MSGINFO_XCAPS_SHIFT) & MSGINFO_XCAPS_MASK;
    minfo.flags = w & MSGINFO_FLAGS_MASK;
    return minfo;
}

inline uint64_t msginfo_word_new(uint32_t label, uint16_t length, uint8_t caps, uint8_t flags)
{
    uint64_t info = 0;
    info = (label & MSGINFO_LABEL_MASK) << MSGINFO_LABEL_SHIFT; 
    info |= (length & MSGINFO_LEN_MASK) << MSGINFO_LEN_SHIFT;
    info |= (caps & MSGINFO_XCAPS_MASK) << MSGINFO_XCAPS_SHIFT;
    info |= (flags & MSGINFO_FLAGS_MASK);
    return info;
}

#define label_from_msginfo_word(w) (((msg_info_t)w >> MSGINFO_LABEL_SHIFT) & MSGINFO_LABEL_MASK) 
#define length_from_msginfo_word(w) (((uint64_t)w >> MSGINFO_LEN_SHIFT) & MSGINFO_LEN_MASK) 
#define xcaps_from_msginfo_word(w) (((uint64_t)w >> MSGINFO_XCAPS_SHIFT) & MSGINFO_XCAPS_MASK)

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
    struct task *task;
};

typedef struct mqueue mqueue_t;

#define IPC_CALL 1
#define IPC_REPL 2

typedef struct ipc_msg
{
    uint64_t    type;
    list_head_t mlist;
    struct task *sender;
    int         reply;
    char message[1];
}ipc_msg_t;


typedef struct ipc_fastcall_msg
{
//    uint64_t    type;
    list_head_t mlist;
    struct task *sender;
    uint64_t    arg[IPC_MAX_REGS];
}ipc_fastcall_msg_t;

int ipc_init(void);
// uint64_t ipc_get_msg(uint64_t qkey, uint64_t flags);
// int ipc_snd_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags);
// int ipc_rcv_msg(uint64_t qid, uint64_t type, uintptr_t ubuf, uint64_t size, uint64_t flags);

// uint64_t ipc_get_shm(uint64_t key, size_t size, uint64_t flags);
// void *ipc_att_shm(uint64_t shmid, const void *addr, int flags);

void *sys_ipc_shm_attach(struct task *t, int cap_id, const void *addr, int flags);

// uint64_t sys_ipc_call(struct task *t, uint32_t id, uint64_t umsg, uint64_t urep, uint64_t size);
// uint64_t sys_ipc_recv(struct task *t, uint32_t id, uint64_t uaddr, uint64_t size, int flags);

// uint64_t sys_ipc_send(struct task *t, uint32_t id, uint64_t uaddr, uint64_t size);
// uint64_t sys_ipc_replay(struct task *t, uint64_t id, uint64_t uaddr, uint64_t size);


int sys_ipc_recieve(struct task *t, int cap_id, bool is_blocking);
int sys_ipc_send(struct task *t, int cap_id, bool is_blocking, bool is_call);
int sys_ipc_reply(struct task *t);

#endif /* __IPC_H__ */