#ifndef __NOTIFICATION_H__
#define __NOTIFICATION_H__

#include <object.h>
#include <spinlock.h>

struct task;

typedef struct notification
{
    kobject_t       hdr;
//    int             padding;
    spinlock_t      lock;
    uint64_t        word;       // badge OR-accumulator, drained on wait
    struct task    *bound_tcb;  // weak ref — cleared on unbind/task death
} notification_t;

int  notification_bind(struct task *t, notification_t *notif);
void notification_unbind(struct task *t);
void notification_signal(notification_t *notif, uint64_t badge);
int  notification_wait(struct task *t, notification_t *notif, bool blocking);
void notification_on_task_exit(struct task *t);

int  sys_notification_create(struct task *t);
int  sys_notification_bind(struct task *t);
int  sys_notification_unbind(struct task *t);
int  sys_notification_signal(struct task *t);
int  sys_irq_bind_notification(struct task *t);

#endif /* __NOTIFICATION_H__ */
