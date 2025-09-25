#include <string.h>

char *strncpy(char *dest, const char *src, size_t count)
{
	char *ret = dest;

	while (count-- && *src != '\0') {
		*dest++ = *src++;
	}

	return ret;
}