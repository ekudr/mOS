#ifndef __ENDPOINT_H__
#define __ENDPOINT_H__

#include <object.h>

struct task;

typedef struct endpoint
{
    struct kobject   ko;
    spinlock_t  lock;
    list_head_t msglist;
    struct task *owner;
    uint64_t    count;
} endpoint_t;

typedef struct fastcall
{
    struct kobject   ko;
    spinlock_t  lock;
    list_head_t tlist;  // list of senders
    struct task *owner; // request handler/owner
    uint64_t    count;
} fastcall_t;

typedef struct replay
{
    struct kobject  ko;
    spinlock_t      lock;
    struct task     *sender; // request sender

} replay_t;

#endif /* __ENDPOINT_H__ */