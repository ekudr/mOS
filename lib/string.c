#include "stddef.h"

/*
void *memset(void *s, int c, size_t count)
{
    char *temp = s;

    while (count > 0) {
        count--;
        *temp++ = c;
    }

    return s;
}


void *memcpy(void *dest, const void *src, size_t count)
{
    char *temp1	  = dest;
    const char *temp2 = src;

    while (count > 0) {
        *temp1++ = *temp2++;
        count--;
    }

    return dest;
}
*/

size_t strlen(const char *str)
{
    unsigned long ret = 0;

    while (*str != '\0')
    {
        ret++;
        str++;
    }

    return ret;
}