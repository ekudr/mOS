#ifndef __COMMON_H__
#define __COMMON_H__

#include <errno.h>
#include <board.h>
#include <board_krnldata.h>
#include <stdint.h>
#include <stddef.h>
#include <printf.h>
#include <string.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define __ALIGN(n) __attribute__ ((aligned (n)))
#define __CACHE_ALIGN __attribute__ ((aligned (64)))

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define BIT(nr)			(1 << (nr))

#if defined(__DEBUG__)
#define debug(...)	printf(__VA_ARGS__)
#else
#define debug(...)
#endif
#define DEBUG	debug

#define panic(str)	panic_("\x1b[31mPANIC STOP:\x1b[0m %s at %s:%d\n", str, __FILE__, __LINE__)

void panic_(const char* format, ...);
int kprint(const char* format, ...);
void sbi_putc(char);


void *malloc(uint64_t size);
void mfree(void *ptr);

uint64_t usec_to_tick(unsigned long usec);
void udelay(unsigned long usec);

int sbi_remote_hfence_vvma(unsigned long start, unsigned long size);
int sbi_remote_sfence_vma(unsigned long start, unsigned long size);

#endif /* __COMMON_H__ */