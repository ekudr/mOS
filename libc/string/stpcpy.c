#include <string.h>
#include <stdint.h>


#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

char *stpcpy(char *restrict d, const char *restrict s)
{

	for (; (*d=*s); s++, d++);

	return d;
}
