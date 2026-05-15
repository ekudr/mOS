#ifndef __LIBSYS_COMMON_H__
#define __LIBSYS_COMMON_H__

#include <stdint.h>
#include <stddef.h>
#include <printf.h>
#include <stdarg.h>
#include <board.h>
#include <errno.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define __ALIGN(n) __attribute__ ((aligned (n)))
#define __CACHE_ALIGN __attribute__ ((aligned (64)))

int debug(const char *format, ...);

char* sbrk(int);

int sys_wait_irq(void);
//uint64_t getpid(void);
int cache_flush(void *addr, size_t size);
int cache_invalidate(void *addr, size_t size);

kerrno_t irq_set(uint64_t irq, uint64_t flags);
kerrno_t irq_act(uint64_t irq, uint64_t flags);

#endif /* __LIBSYS_COMMON_H__ */