#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

void *memset(void *s, int c, size_t count);
void *memmove(void *dest, const void *src, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
size_t strlen(const char *str);

#endif /* __STRING_H__ */