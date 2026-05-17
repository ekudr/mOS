#include <common.h>
#include <sched.h>
#include <khash.h>
#include <irq.h>
#include <notification.h>

irq_manager_t   *gp_irqm;

kerrno_t irq_set(int irq, task_t *t, int flags)
{
    if (t == NULL)
        return -ENOENT;

    irq_entry_t *e = malloc(sizeof(irq_entry_t));
    if (e == NULL)
        return -ENOMEM;
    
    e->hartid = current_cpu->hartid;
    e->irq    = irq;
    e->task   = t;

    e->notif = NULL;
    e->badge = 0;

//    debug("IRQ set 0x%lX for irq %d\n", t, irq); 
    acquire(&gp_irqm->lock);

    khash_insert(gp_irqm->irq_table, irq, e);
    plic_irq_enable(e->hartid, irq);
    list_add(&gp_irqm->irqlist, &e->irqlist);
//    debug("HASH contains 0x%lX\n", khash_lookup(irq_table, irq));
    release(&gp_irqm->lock);
    return SUCCESS;
}

kerrno_t irq_act(int irq, task_t *t, int flags)
{
    irq_entry_t *e = khash_lookup(gp_irqm->irq_table, irq);
//    debug("[IRQ] act irq %d task %d entry 0x%lX\n", irq, t->pid, e);
    if (e == NULL)
        return -ENOENT;
           
    plic_irq_enable(e->hartid, irq);
    plic_irq_complete(e->hartid, irq); 
    return SUCCESS;
}

kerrno_t irq_bind_notification(int irq, notification_t *notif, uint64_t badge)
{
    acquire(&gp_irqm->lock);
    irq_entry_t *e = khash_lookup(gp_irqm->irq_table, irq);
    release(&gp_irqm->lock);

    if (e == NULL) return -ENOENT;

    // Drop old notification ref if re-binding
    if (e->notif != NULL)
        ko_put((kobject_t *)e->notif, CAP_NOTIFICATION, NULL);

    e->notif  = (notification_t *)ko_get((kobject_t *)notif);
    e->badge  = badge;
    return SUCCESS;
}

kerrno_t irq_send_signal(int irq)
{
    irq_entry_t *e = khash_lookup(gp_irqm->irq_table, irq);

    if ((e == NULL) || (e->task == NULL))
        return -ENOENT;

    // Prefer notification delivery if bound
    if (e->notif != NULL) {
        notification_signal(e->notif, e->badge ? e->badge : 1ULL);
        return SUCCESS;
    }

    return signal_send(e->task, SIGNAL_IRQ, irq);
}

void irq_init(void)
{
    gp_irqm = malloc(sizeof(irq_manager_t));
    if (gp_irqm == NULL)
        panic("[IRQ] cannot allocate irq manager");
    initlock(&gp_irqm->lock, "irq manager");
    list_init(&gp_irqm->irqlist);
    gp_irqm->irq_table =khash_create(5);

}