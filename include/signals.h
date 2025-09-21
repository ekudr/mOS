#ifndef __SIGNALS_H__
#define __SIGNALS_H__

struct task;

typedef uint32_t signal_t;
typedef uint64_t signal_payload_t;
typedef void*    signal_handler_t;

enum {
    SIGNAL_IRQ = 1,
    SIGNAL_TIMER,
    SIGNAL_SHM_READY,
    SIGNAL_CONSOLE,

    SIGNAL_USER_BASE = 0x30,   // user-defined start
};


#define MAX_NR_SIGNAL 64

typedef struct signal_entry
{
    signal_t         signal;     
    signal_payload_t payload; 
    list_head_t      siglist;  
} signal_entry_t;

// Signal queue 
typedef struct signal_queue 
{
    unsigned count; // number of elements ??? needs
    list_head_t siglist;
} signal_queue_t;

typedef struct signal_action
{
    signal_handler_t handler;   // Handler 
    signal_handler_t restorer;  // Restorer  
} signal_action_t;


typedef struct signal_hand
{
    spinlock_t  lock;
    /* pending mask (bitset): quick check if non-queued signals pending */
    uint64_t                pending_mask;
    /* mask of allowed/blocked types (1=blocked )*/
    uint64_t                blocked_mask;

    struct signal_action    sa[MAX_NR_SIGNAL];
    struct signal_queue     queue;
    /* waiter/wake list: tasks blocked on notification */
    list_head_t wq;
} signal_hand_t;


kerrno_t signal_action(signal_t sig, uintptr_t user_sa);
kerrno_t signal_send(struct task *dst_task, signal_t sig, signal_payload_t payload);
kerrno_t signal_getnext(struct task *task, signal_entry_t *out);

#endif /* __SIGNALS_H__ */