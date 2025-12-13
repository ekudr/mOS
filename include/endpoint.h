#ifndef __ENDPOINT_H__
#define __ENDPOINT_H__

#include <object.h>

struct task;

enum endpoint_status {
    EP_STATE_IDLE,
    EP_STATE_RECV,
    EP_STATE_SEND,
};

typedef struct endpoint
{
    struct kobject   hdr;
    int         state;    
    spinlock_t  lock;

//    list_head_t msglist; // del
    list_head_t queue;
    struct task *owner;
    uint64_t    badge;
    uint64_t    count;
} endpoint_t;

// typedef struct fastcall
// {
//     struct kobject   hdr;
//     spinlock_t  lock;
//     int         state;
//     list_head_t tlist;  // list of senders
//     struct task *owner; // request handler/owner
//     uint64_t    count;
// } fastcall_t;

typedef struct replay
{
    struct kobject  hdr;
//    spinlock_t      lock;
    struct task     *sender; // request sender

} replay_t;

#endif /* __ENDPOINT_H__ */