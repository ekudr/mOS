#ifndef __LIBSYS_TIMER_H__
#define __LIBSYS_TIMER_H__

/* Returns time in milliseconds */
uint64_t get_timer(uint64_t base);

uint64_t usec_to_tick(unsigned long usec);

void udelay(unsigned long usec);

#endif /* __LIBSYS_TIMER_H__ */