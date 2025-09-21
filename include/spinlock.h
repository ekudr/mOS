
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

// Mutual exclusion lock.
struct spinlock {
    union
    {  
        volatile unsigned int counter;
        struct {
            volatile unsigned short now_serving;
            volatile unsigned short next_ticket;         
        } c;
    };

    // For debugging:
    char *name;        // Name of lock.
    struct cpu *cpu;   // The cpu holding the lock.
};

typedef struct spinlock spinlock_t;

void initlock(spinlock_t *lk, char *name);
int acquire(spinlock_t *lk);
void release(spinlock_t *lk);
void push_off(void);
void pop_off(void);
int holding(struct spinlock *lk);


#endif  /* _SPINLOCK_H */