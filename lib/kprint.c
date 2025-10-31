#include <common.h>
#include <string.h>

int sbi_debug_console_write(const char *bytes, unsigned int num_bytes);

int kprint(const char *format, ...)
{
    char *buffer = (char *)PPN2DA(pgalloc());
    va_list va;
    va_start(va, format);
    
    const int ret = vsnprintf(buffer, 4096, format, va);
    int c = strlen(buffer);
    sbi_debug_console_write(buffer, c);
    va_end(va);
    pgfree(DA2PPN(buffer));
    return ret;
}

void panic_(const char* format, ...)
{
    char *buffer = (char *)PPN2DA(pgalloc());
    va_list va;
    va_start(va, format);
    
    vsnprintf(buffer, 4096, format, va);
    int c = strlen(buffer);
    sbi_debug_console_write(buffer, c);
    va_end(va);
    pgfree(DA2PPN(buffer));
//    intr_off();
    for(;;);
}