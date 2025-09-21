
#include <stdarg.h>
#include <printf.h>

int sys_debug(char *msg);


int debug(const char *format, ...)
{
    
    va_list va;
    va_start(va, format);
    char buffer[256];
    const int ret = vsnprintf(buffer, 256, format, va);
    sys_debug(buffer);
    va_end(va);
    return ret;
}
