#ifndef	__MOSSTD_H__
#define	__MOSSTD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/memory.h>
#include <stdaligns.h>

#include <nameserver.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

pid_t getpid(void);

int debug(const char *format, ...);
int cons_out(const char *format, ...);

int  snprintf_(char* buffer, size_t count, const char* format, ...);

void *malloc(size_t size);
void free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* __MOSSTD_H__ */