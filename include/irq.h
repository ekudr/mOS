#ifndef __IRQ_H__
#define __IRQ_H__

#include <khash.h>
#include <object.h>

struct notification;

typedef struct irq_entry
{
    int                    irq;
    int                    hartid;
    struct task           *task;
    list_head_t            irqlist;
    struct notification   *notif;   // if non-NULL, deliver via notification_signal
    uint64_t               badge;   // badge ORed into notif->word on IRQ
} irq_entry_t;


typedef struct irq_manager
{
    spinlock_t      lock;
    khash_table_t   *irq_table;
    list_head_t     irqlist;
} irq_manager_t;


typedef struct irqpoint
{
    struct kobject      ko;
    spinlock_t          lock;
    struct task        *owner;
    int                 irq;
    struct notification *notif;
    uint64_t            badge;
} irqpoint_t;


void irq_init(void);
kerrno_t irq_set(int irq, task_t *t, int flags);
kerrno_t irq_act(int irq, task_t *t, int flags);
kerrno_t irq_bind_notification(int irq, struct notification *notif, uint64_t badge);
void plic_irq_enable(uint64_t hartid, uint32_t irq);
void plic_irq_disable(uint64_t hartid, uint32_t irq);
kerrno_t irq_send_signal(int irq);
void plic_cpu_enable(uint64_t hartid);
void plic_irq_complete(uint64_t hartid, uint32_t irq);


#endif /* __IRQ_H__ */