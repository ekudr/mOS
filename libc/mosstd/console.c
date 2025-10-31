#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <libsys/printf.h>
#include <mosstd.h>
#include <errno.h>
#include <ipc.h>
#include <vfs.h>
#include <tty.h>


int tty_cap = 0;


void open_console()
{
    do {
        tty_cap = vfs_open("/dev/tty0");
    } while (tty_cap <=0);
} 

int console_puts(const char *str)
{
    int ret;
    struct tty_message cout;

    if (unlikely(!tty_cap)) open_console();

    memset(&cout, 0, sizeof(cout));
    strncpy(cout.message, str, sizeof(cout.message)-1);
    cout.type = TTY_PUT_STRING;
    do {
    ret = ipc_send(tty_cap, &cout, sizeof(cout));
    } while (ret == -EAGAIN);

    return ret;
}

int cons_out(const char *format, ...)
{    
    va_list va;
    va_start(va, format);
    char buffer[100];
    const int ret = vsnprintf(buffer, 100, format, va);
    console_puts(buffer);
    va_end(va);
    return ret;
}
