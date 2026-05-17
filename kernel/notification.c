#include <common.h>
#include <notification.h>
#include <cap.h>
#include <ipc.h>
#include <irq.h>
#include <sched.h>
#include <sysproc.h>

static void notification_destroy(kobject_t *ko, uint32_t type)
{
    notification_t *notif = (notification_t *)ko;
    // bound_tcb must be NULL by the time refcount → 0
    // (ensured by notification_unbind / notification_on_task_exit)
    mfree(notif);
}

/*
 * notification_signal — called from IRQ context or syscall.
 * Atomically ORs badge into word.  If the bound TCB is blocked,
 * drains word into t->notif_word and makes it RUNNABLE.
 * IRQ-safe: acquires notif->lock (push_off/pop_off via acquire).
 */
void notification_signal(notification_t *notif, uint64_t badge)
{
    acquire(&notif->lock);
    __atomic_fetch_or(&notif->word, badge, __ATOMIC_ACQ_REL);

    task_t *t = notif->bound_tcb;
    if (t != NULL) {
        acquire(&t->lock);
        uint32_t state = get_task_state(t);
        if (state == BLOCKED_RECV) {
            // Bound thread waiting in ipc_recv — drain into notif_word and wake.
            // The ipc_recv return path checks notif_word before returning IPC data.
            t->notif_word = __atomic_exchange_n(&notif->word, 0, __ATOMIC_ACQ_REL);
            t->chan = 0;
            set_task_state(t, RUNNABLE);
        } else if (state == SLEEPING && t->chan == (void *)notif) {
            // Thread blocked in notification_wait (direct wait, chan == notif).
            t->notif_word = __atomic_exchange_n(&notif->word, 0, __ATOMIC_ACQ_REL);
            t->chan = 0;
            set_task_state(t, RUNNABLE);
        }
        release(&t->lock);
    }
    release(&notif->lock);
}

/*
 * notification_wait — called from sys_ipc_recieve when cap type is CAP_NOTIFICATION.
 * Drains word if available; blocks (SLEEPING) if not.
 */
int notification_wait(task_t *t, notification_t *notif, bool blocking)
{
    acquire(&notif->lock);

    uint64_t w = __atomic_exchange_n(&notif->word, 0, __ATOMIC_ACQ_REL);
    if (w != 0) {
        release(&notif->lock);
        syscall_set_MR(t, 0, w);
        syscall_set_MR(t, 1, msginfo_word_new(0, 0, 0, MSGINFO_NOTIFICATION));
        return SUCCESS;
    }

    if (!blocking) {
        release(&notif->lock);
        syscall_set_MR(t, 0, 0);
        return -EAGAIN;
    }

    // No pending word — block.  Temporarily use bound_tcb if unset.
    if (notif->bound_tcb != NULL && notif->bound_tcb != t) {
        release(&notif->lock);
        return -EBUSY;
    }
    notif->bound_tcb = t;
    // sched_task_block releases notif->lock, blocks, re-acquires on wakeup
    sched_task_block(notif, &notif->lock, SLEEPING);

    // Woke up — drain notif_word staged by notification_signal
    uint64_t nw = t->notif_word;
    t->notif_word = 0;
    // Clear temporary bind if still pointing to us
    if (notif->bound_tcb == t) notif->bound_tcb = NULL;
    release(&notif->lock);

    syscall_set_MR(t, 0, nw);
    syscall_set_MR(t, 1, msginfo_word_new(0, 0, 0, MSGINFO_NOTIFICATION));
    return SUCCESS;
}

/*
 * notification_bind — bind notif to TCB t.
 * Takes an extra ko_get ref so the notif stays alive until explicit unbind
 * or task death, even if all user caps are dropped.
 */
int notification_bind(task_t *t, notification_t *notif)
{
    if (t->bound_notif != NULL) return -EEXIST;

    acquire(&notif->lock);
    if (notif->bound_tcb != NULL) {
        release(&notif->lock);
        return -EEXIST;
    }
    notif->bound_tcb = t;
    release(&notif->lock);

    t->bound_notif = (notification_t *)ko_get((kobject_t *)notif);
    return SUCCESS;
}

/*
 * notification_unbind — remove binding between t and its bound notification.
 */
void notification_unbind(task_t *t)
{
    notification_t *notif = t->bound_notif;
    if (notif == NULL) return;

    acquire(&notif->lock);
    if (notif->bound_tcb == t) notif->bound_tcb = NULL;
    release(&notif->lock);

    t->bound_notif = NULL;
    ko_put((kobject_t *)notif, CAP_NOTIFICATION, notification_destroy);
}

/*
 * notification_on_task_exit — called by task destructor before freeing t.
 */
void notification_on_task_exit(task_t *t)
{
    notification_unbind(t);
}

/* ───── Syscall handlers ───── */

int sys_notification_create(task_t *t)
{
    uint32_t rights = syscall_get_MR(t, 1);
    if (!rights) return -EINVAL;

    notification_t *notif = malloc(sizeof(notification_t));
    if (notif == NULL) return -ENOMEM;

    ko_init((kobject_t *)notif, t, KO_NOTIFICATION);
    initlock(&notif->lock, "notification");
    notif->word      = 0;
    notif->bound_tcb = NULL;

    int ret = cap_install(t, notif, CAP_NOTIFICATION, rights);
    if (ret < 0) {
        mfree(notif);
        return ret;
    }
    return ret;
}

int sys_notification_bind(task_t *t)
{
    int notif_cap = (int)syscall_argraw(0);

    cap_entry_t *ce = cap_lookup(t, notif_cap);
    if (!ce) return -ERR_CAP_INVAL;
//    debug("\x1b[31mNOTIF\x1b[0m type %d rights 0x%X\n", ce->type, ce->rights);
    if (ce->type != CAP_NOTIFICATION || !(ce->rights & CRIGHT_NOTIFY))
        return -EPERM;

    return notification_bind(t, (notification_t *)ce->obj);
}

int sys_notification_unbind(task_t *t)
{
    notification_unbind(t);
    return SUCCESS;
}

int sys_notification_signal(task_t *t)
{
    int cap_id = (int)syscall_argraw(0);

    cap_entry_t *ce = cap_lookup(t, cap_id);
    if (!ce) return -ERR_CAP_INVAL;
    if (ce->type != CAP_NOTIFICATION || !(ce->rights & CRIGHT_SND))
        return -EPERM;

    notification_signal((notification_t *)ce->obj, ce->badge);
    return SUCCESS;
}

/*
 * sys_irq_bind_notification — bind an IRQ number to a notification cap.
 * After this, irq_send_signal() delivers via notification_signal() instead
 * of the legacy signal_send() path.
 * a0 = irq number, a1 = notif_cap_id
 */
int sys_irq_bind_notification(task_t *t)
{
    int irq          = (int)syscall_argraw(0);
    int notif_cap_id = (int)syscall_argraw(1);

    cap_entry_t *notif_ce = cap_lookup(t, notif_cap_id);
    if (!notif_ce) return -ERR_CAP_INVAL;
    if (notif_ce->type != CAP_NOTIFICATION || !(notif_ce->rights & CRIGHT_SND))
        return -EPERM;

    notification_t *notif = (notification_t *)notif_ce->obj;
    uint64_t badge = notif_ce->badge;

    return irq_bind_notification(irq, notif, badge);
}
