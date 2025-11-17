#include <string.h>

char *strchrnul(const char *s, int c);

char *strchr(const char *s, int c)
{
	char *r = strchrnul(s, c);
	return *(unsigned char *)r == (unsigned char)c ? r : 0;
}