#include <common.h>
#include <riscv.h>


/*
static inline uint64_t
csr_read_time(void)
{
	uint64_t x;
	asm volatile ("csrr %0, 0xc01" : "=r" (x) :	  : "memory");
	return x;
}
*/

static uint64_t timer_get_count(void)
{
	return r_time();
}

/* Returns time in milliseconds */
static uint64_t tick_to_time(uint64_t tick)
{
	uint64_t div = CONFIG_SYS_TIMER_RATE; //get_tbclk();

	tick *= 1000;  //CONFIG_SYS_HZ
	tick /= div;
	return tick;
}

/* Returns time in milliseconds */
uint64_t get_timer(uint64_t base)
{
	return tick_to_time(timer_get_count()) - base;
}

uint64_t usec_to_tick(unsigned long usec)
{
	uint64_t tick = usec;
	tick *= CONFIG_SYS_TIMER_RATE; 
	tick /= 1000000;
	return tick;
}

// rewrite
// NOT usec.  count ticks
void udelay(unsigned long usec)
{

	uint64_t tmp;

	tmp = timer_get_count() + usec_to_tick(usec);	/* get current timestamp */

	while (timer_get_count() < tmp+1)	/* loop till event */
		 /*NOP*/;
}