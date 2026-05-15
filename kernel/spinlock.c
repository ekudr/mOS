#include <common.h>
#include <spinlock.h>
#include <riscv.h>
#include <sched.h>


static inline int
arch_atomic_fetch_add(int i, spinlock_t *l)
{
	register int ret;
	__asm__ __volatile__ (
		"amoadd.w.aqrl  %1, %2, %0\n"
		: "+A" (l->counter), "=r" (ret)
		: "r" (i)
		: "memory");
	return ret;
}

void push_off(void)
{
     int old = intr_get();

    intr_off();
    if(current_cpu->noff == 0)
        current_cpu->intena = old;
    current_cpu->noff += 1;
}

void pop_off(void)
{
    if(intr_get())
        panic("[SHED] pop_off - interruptible");
    if(current_cpu->noff < 1)
        panic("[SHED] Lock pop_off");
    current_cpu->noff -= 1;
    if(current_cpu->noff == 0 && current_cpu->intena)
        intr_on();
}


void 
initlock(spinlock_t *lk, char *name)
{
    lk->name = name;
    lk->c.now_serving = 0;
    lk->c.next_ticket = 0;
    lk->cpu = 0;
}

int 
acquire (spinlock_t *lk) {
    int spin = 0;

    push_off();
/*
//   uint32_t val = arch_atomic_fetch_add(1<<16, lk);
    uint32_t val = (uint32_t)__atomic_fetch_add(&lk->counter, (1<<16), __ATOMIC_ACQ_REL);
    uint16_t ticket = val >> 16;

//    printf("My ticket %d, next %d, serving %d\n",ticket, lk->c.next_ticket, lk->c.now_serving);
//    my_ticket = __sync_fetch_and_add(&(lk->next_ticket), 1);
    
    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    while (lk->c.now_serving != ticket)
        spin++;
*/
//    debug("Counter before lock 0x%lX\n", lk->counter);
    uint32_t val = (uint32_t)__atomic_fetch_add(&lk->counter, (1<<16), __ATOMIC_RELAXED);
    uint16_t ticket = val >> 16;
//    printf("My ticket %d, next %d, serving %d val %p\n",ticket, lk->c.next_ticket, lk->c.now_serving, val);
    while (ticket != __atomic_load_n(&lk->c.now_serving, __ATOMIC_ACQUIRE))
        spin++;
//    debug("Counter after lock 0x%lX\n", lk->counter);
    lk->cpu = current_cpu;
    return(spin);
}

// Release the lock.
void 
release(struct spinlock *lk)
{
    if(!holding(lk)) {
        debug("[SPINLOCK] Lock %s release\n", lk->name);
        panic("[SPINLOCK] Lock release\n");
    }
    lk->cpu = 0;
     uint16_t s = __atomic_load_n(&lk->c.now_serving, __ATOMIC_ACQUIRE);
    __atomic_store_n(&lk->c.now_serving, s+1, __ATOMIC_RELEASE);
    pop_off();
}

/*
 * Check whether this cpu is holding the lock.
 * Interrupts must be off.
*/
int holding(struct spinlock *lk)
{
    return ((lk->c.next_ticket != lk->c.now_serving)&& lk->cpu == current_cpu);
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.
