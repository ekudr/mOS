#include <common.h>
#include <sched.h>
#include <signals.h>
/* --------------------------- Helpers ----------------------------------- */

/* compute mask for type t */
static inline uint64_t sig_mask(signal_t s)
{
    if (s < 0 || s >= (int)MAX_NR_SIGNAL) return 0;

    return (1ULL << s);
}

/* ------------------------ Queue operations ------------------------------ */


static inline bool queue_is_empty(struct signal_queue *q)
{
    return list_is_empty(&q->siglist);
}

static inline int queue_push(struct signal_queue *q, signal_t sig, signal_payload_t payload)
{
    
    signal_entry_t *e = malloc(sizeof(signal_entry_t));
//    debug("Allocated entry 0x%lX\n", e);
    if (e == NULL)
        return -ENOMEM;
    e->signal   = sig;
    e->payload  = payload;
    list_add_tail(&q->siglist, &e->siglist);
    q->count++;
//    debug("Entry 0x%lX push %d payld %d\n", e, e->signal, e->payload);
    return SUCCESS;
}

static inline int queue_pop(struct signal_queue *q, signal_entry_t *out)
{
    signal_entry_t *e;

    if (queue_is_empty(q)) 
        return -ENOENT;
//    debug("poping out\n");
    if (out) {
        e = list_first_entry(&q->siglist, signal_entry_t, siglist);
        *out = *e;
        list_del(&e->siglist);
        q->count--;
    }
        
//    debug("Entry 0x%lX pop %d payld %d\n", e, e->signal, e->payload);
    return 0;
}

/*
 * Set signal action
 * Input signal, signal_action pointer in user space.
 */
kerrno_t signal_action(signal_t sig, uintptr_t user_sa)
{
    signal_action_t *sa;
    task_t  *t = mytask();
//    debug("[SIGACT] set sig %d sa 0x%lX for task %d\n", sig, user_sa, t->pid);
    if ((sig == 0) || (user_sa == 0))
        return -EINVAL;
    
    sa = malloc(sizeof(signal_action_t));
//    debug("SA allocated 0x%lX\n", sa);
    if (sa == NULL)
        return -ENOMEM;
    
    mmu_user_copyin(t->pagetable, (char *)sa, user_sa, sizeof(signal_action_t));

    // create an empty signal-handler table if not exist 
    if (unlikely(t->sighand == NULL)) {
        t->sighand = malloc(sizeof(signal_hand_t));
        if (t->sighand == NULL)
            return -ENOMEM;
        memset(t->sighand, 0, sizeof(signal_hand_t));
        initlock(&t->sighand->lock, "Sighand");
        list_init(&t->sighand->queue.siglist);
    }

    debug("[SIGACT] set sig %d sa handler 0x%lX restorer 0x%lX\n", sig, sa->handler, sa->restorer);
    acquire(&t->sighand->lock);
    t->sighand->sa[sig].handler = sa->handler;
    t->sighand->sa[sig].restorer = sa->restorer;
//    debug("         sig %d handler 0x%lX\n", sig, t->sighand->sa[sig].handler);
    release(&t->sighand->lock);

    mfree(sa);
    return SUCCESS;
}

/*
 * Send a signal to the task
 */
kerrno_t signal_send(task_t *dst_task, signal_t sig, signal_payload_t payload)
{
    signal_hand_t *th = dst_task->sighand;
//    debug("sent signal %d payload %d th 0x%lX\n", sig, payload, th);
    if (th == NULL) 
        return -ENOENT;
    uint64_t mask = sig_mask(sig);
    if (mask == 0) return -EINVAL;

    acquire(&th->lock);
    
    /* if the signal is blocked, mark pending and return */
    if ((th->blocked_mask & mask) != 0) {
        __atomic_fetch_or(&th->pending_mask, mask, __ATOMIC_ACQ_REL);
        /* we do not enqueue blocked notifications, but they are remembered in pending_mask */
        release(&th->lock);
        return 0;
    }

    /* try enqueue */

    queue_push(&th->queue, sig, payload);
    __atomic_fetch_or(&th->pending_mask, mask, __ATOMIC_ACQ_REL);

//    debug("blocked signals 0x%lX pending 0x%lX\n", th->blocked_mask, th->pending_mask);
//    sched_task_wakeup(&ts->wq);
    release(&th->lock);
    return SUCCESS;
}

kerrno_t signal_getnext(task_t *task, signal_entry_t *out)
{
    signal_hand_t   *th = task->sighand;

    if (th == NULL) 
        return -ENOENT;
    if (out == NULL)
        return -EINVAL;
//    debug("task %d th 0x%lX\n", task->pid, th);
    acquire(&th->lock);
    queue_pop(&th->queue, out);

    uint64_t mask = sig_mask(out->signal);
    __atomic_fetch_and(&th->pending_mask, ~mask, __ATOMIC_ACQ_REL);

    release(&th->lock);

    return SUCCESS;
}