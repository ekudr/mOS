#ifndef __LIBSYS_COMMON_H__
#define __LIBSYS_COMMON_H__

#include <stdint.h>
#include <stddef.h>
#include <printf.h>
#include <stdarg.h>
#include <board.h>
#include <errno.h>



int debug(const char *format, ...);

char* sbrk(int);
//uint64_t getpid(void);
int cash_flash(void *addr, size_t size);

kerrno_t irq_set(uint64_t irq, uint64_t flags);
kerrno_t irq_act(uint64_t irq, uint64_t flags);

#endif /* __LIBSYS_COMMON_H__ */