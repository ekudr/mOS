/* --------------------------- Helpers ----------------------------------- */
/* Map notif type to bit index: we require type < (1<<20) typically.
* For simplicity, we map small type numbers directly to bit = (type & 63).
* In production you'd have a type->bit registry. */
static inline int sig_to_bit(signal_t s)
{
    /* We reserve 1..63 directly, and user signals must be within 0..63 */
    if (s == 0) return -EINVAL;
    /* map small well-known types to low bits */
    if (s < MAX_NR_SIGNAL) return (int)s; /* bit 1..63 (we'll subtract 1 in mask) */
    /* For larger types, fold into 0..63 (not ideal) */
    return (int)(s % MAX_NR_SIGNAL);
}

/* compute mask for type t */
static inline uint64_t type_mask(signal_t t)
{
    int b = sig_to_bit(t);
    if (b < 0 || b >= (int)MAX_NR_SIGNAL) return 0;

    return (1ULL << b);
}

/* ------------------------ Queue operations ------------------------------ */
/*
static inline bool queue_is_full(struct signal_queue *q)
{
    return q->count >= SIGNAL_QUEUE_SIZE;
}

static inline bool queue_is_empty(struct signal_queue *q)
{
    return q->count == 0;
}

static inline int queue_push(struct signal_queue *q, signal_t type, signal_payload_t payload)
{
    if (queue_is_full(q))
        return -1;
    q->slots[q->head].type = type;
    q->slots[q->head].payload = payload;
    q->head = (q->head + 1) % SIGNAL_QUEUE_SIZE;
    q->count++;
    return 0;
}

static inline int queue_pop(struct signal_queue *q, struct signal_entry *out)
{
    if (queue_is_empty(q)) 
        return -1;
    if (out) 
        *out = q->slots[q->tail];
    q->tail = (q->tail + 1) % SIGNAL_QUEUE_SIZE;
    q->count--;
    return 0;
}
*/
/* optional: drop oldest 
static inline void queue_drop_oldest(struct signal_queue *q)
{
    if (!queue_is_empty(q)) {
        q->tail = (q->tail + 1) % SIGNAL_QUEUE_SIZE;
        q->count--;
    }
}
*/
/*
* Signal_send - send a signal to dst task
* - returns 0 on success
* - -ENOSPC if queue full (policy: drop on full)
* - -ENOENT if target not found
*/
kerrno_t signal_send(task_t *dst_task, signal_t type, signal_payload_t payload)
{
    task_signal_t *ts = dst_task->signal;
    if (ts == NULL) 
        return -ENOENT;
    uint64_t mask = type_mask(type);
    if (mask == 0) return -EINVAL;

    acquire(&ts->lock);
    /* if the type is blocked, mark pending and return */
    if ((ts->blocked_mask & mask) != 0) {
        __atomic_fetch_or(&ts->pending_mask, mask, __ATOMIC_ACQ_REL);
        /* we do not enqueue blocked notifications, but they are remembered in pending_mask */
        release(&ts->lock);
        return 0;
    }

    /* try enqueue */
//    if (!queue_is_full(&ts->queue)) {
//        queue_push(&ts->queue, type, payload);
//        __atomic_fetch_or(&ts->pending_mask, mask, __ATOMIC_ACQ_REL);
//    } else {
        /* queue full: policy -> drop and signal ENOSPC */
//        release(&ts->lock);
//        return -ENOSPC;
//    }

//    sched_task_wakeup(&ts->wq);
    release(&ts->lock);
    return SUCCESS;
}


/*
* signal_trywait - non-blocking pop
* - returns 0 if popped, -EAGAIN if none
*/
/*
kerrno_t signal_trywait(task_t *t, signal_t *out_type, signal_payload_t *out_payload)
{
    task_signal_t *ts = &t->signal;
    signal_entry_t e;

    if (ts == NULL) 
        return -ENOENT;

    acquire(&ts->lock);

    if (queue_pop(&ts->queue, &e) == 0) {
//        /* adjust pending_mask: if queue emptied, we might need to clear that bit if
        * there are no more queued entries of that type. For simplicity, we clear the bit.
//        * More precise handling would inspect queue contents. */
        __atomic_fetch_and(&ts->pending_mask, ~type_mask(e.type), __ATOMIC_ACQ_REL);
        release(&ts->lock);
        if (out_type) 
            *out_type = e.type;
        if (out_payload) 
            *out_payload = e.payload;
        return SUCCESS;
    }

    /* no queued entry, check pending_mask for non-queued bits (rare here) */
    uint64_t pm = __atomic_load_n(&ts->pending_mask, __ATOMIC_ACQUIRE);
    if (pm) {
        /* find lowest set bit */
        int bit; // = __builtin_ctzll(pm);
        uint64_t bitmask = 1ULL << bit;
        /* clear it */
        __atomic_fetch_and(&ts->pending_mask, ~bitmask, __ATOMIC_RELEASE);
        release(&ts->lock);
        if (out_type) 
            *out_type = (signal_t)bit; /* reverse mapping is simplistic */
        if (out_payload) 
            *out_payload = 0;
        return SUCCESS;
    }
    release(&ts->lock);
    return -EAGAIN;
}

/*
* signal_wait - blocking wait for a notification
* - timeout_ms < 0 => infinite
* - returns 0 on success (populated out_type/out_payload), -ETIMEDOUT or errno otherwise
*/
kerrno_t signal_wait(task_t *t, signal_t *out_type, signal_payload_t *out_payload, int timeout_ms)
{
    task_signal_t *ts = &t->signal;

    if (ts == NULL) 
        return -ENOENT;

//    struct timespec now, deadline;
    int rc = 0;

    acquire(&ts->lock);

    /* try immediate */
    if (queue_pop(&ts->queue, NULL) == 0) {
        /* pop properly */
        signal_entry_t e;
        /* we popped earlier but popped content was discarded, so re-pop */
        /* reset to correct behavior: pop once properly */
        /* We should not have called queue_pop twice - correct implementation below */
    }
    for (;;) {

        /* prefer queued entries */
        struct signal_entry e;
        if (queue_pop(&ts->queue, &e) == 0) {
            /* clear bit from pending mask (simple approach) */
            __atomic_fetch_and(&ts->pending_mask, ~type_mask(e.type), __ATOMIC_ACQ_REL);
            release(&ts->lock);
            if (out_type) 
                *out_type = e.type;
            if (out_payload) 
                *out_payload = e.payload;
            return SUCCESS;
        }
        /* no queued entry - check pending_mask bits (e.g., blocked notifications that were marked
        pending) */
        uint64_t pm = __atomic_load_n(&ts->pending_mask, __ATOMIC_ACQUIRE);
        if (pm) {
            int bit; // = __builtin_ctzll(pm);
            uint64_t bitmask = 1ULL << bit;
            __atomic_fetch_and(&ts->pending_mask, ~bitmask, __ATOMIC_RELEASE);
            release(&ts->lock);
            if (out_type) 
                *out_type = (signal_t)bit; /* simplistic mapping */
            if (out_payload) 
                *out_payload = 0;
            return SUCCESS;
        }
        /* nothing available: block with timeout if provided */
        if (timeout_ms < 0) {
            sched_task_sleep(&ts->wq, &ts->lock);
            /* loop and recheck */
        } else {
            /* compute deadline */
//            clock_gettime(CLOCK_REALTIME, &now);
//            deadline = now;
//            deadline.tv_sec += timeout_ms / 1000;
////            deadline.tv_nsec += (timeout_ms % 1000) * 1000000;
        //     if (deadline.tv_nsec >= 1000000000) {
        //         deadline.tv_sec++;
        //         deadline.tv_nsec -= 1000000000;
        //     }
        //     int pres = pthread_cond_timedwait(&tn->cond, &tn->lock, &deadline);
        //     if (pres == ETIMEOUT) {
        //         release(&ts->lock);
        //         return -ETIMEOUT;
        //     }
        // /* loop and recheck */
         }
    }
    /* unreachable */
    release(&ts->lock);
    return rc;
}

/* 
 * signal_mask - atomically set/clear blocked bits; returns old mask in old_mask if non-NULL.
 * set_mask bits are turned on (block); clear_mask bits are turned off (unblock).
*/
kerrno_t signal_mask(task_t *t, uint64_t set_mask, uint64_t clear_mask, uint64_t *old_mask)
{
    task_signal_t *ts = &t->signal;

    if (ts == NULL) 
        return -ENOENT;

    acquire(&ts->lock);
    uint64_t old = ts->blocked_mask;
    ts->blocked_mask = (ts->blocked_mask | set_mask) & ~clear_mask;
    release(&ts->lock);
    if (old_mask) 
        *old_mask = old;
    return SUCCESS;
}

/* 
 * notif_peek - copy up to max entries from the queue without removing them.
 * Returns number copied.
*/
int signal_peek(task_t *t, signal_entry_t *out, unsigned max)
{
    task_signal_t *ts = &t->signal;

    if (ts == NULL) 
        return -ENOENT;

    if (max == 0) 
        return 0;

    acquire(&ts->lock);

    unsigned n = ts->queue.count;
    if (n > max) 
        n = max;
    unsigned idx = ts->queue.tail;
    for (unsigned i = 0; i < n; i++) {
        out[i] = ts->queue.slots[idx];
        idx = (idx + 1) % SIGNAL_QUEUE_SIZE;
    }
    release(&ts->lock);
    return (int)n;
}
*/