#include <string.h>

char *stpcpy(char *restrict d, const char *restrict s);

char *strcpy(char *restrict dest, const char *restrict src)
{
	stpcpy(dest, src);
	return dest;
}