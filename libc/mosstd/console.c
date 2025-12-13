#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <libsys/printf.h>
#include <libsys/ipc.h>
#include <mosstd.h>
#include <errno.h>
#include <sched.h>
#include <vfs.h>
#include <tty.h>


int tty_cap = 0;
uint32_t id = 0;

void open_console()
{
    if (!tty_cap) {
        while(1) {
            tty_cap = vfs_open("/dev/con0");
            if (tty_cap > 0) break;
            
            sched_yield();
        }
    }
} 

int console_puts(const char *str)
{
    struct tty_message *cout;

    if (unlikely(!tty_cap)) open_console();

    cout = (struct tty_message *)get_ipc_buffer()->msg;
    memset(cout, 0, sizeof(*cout));
    strncpy(cout->message, str, sizeof(cout->message)-1);
    cout->type = TTY_PUT_STRING;
//    debug("%s", str);
//    memcpy(get_ipc_buffer()->msg, &cout, sizeof(cout));
//    debug("\x1b[31m[CONS]\x1b[0m size of msg 0x%lX\n", sizeof(struct tty_message)); 
    msg_info_t info = msginfo_word_new(id++,(sizeof(struct tty_message)>>3)+1, 0, 0);
//    debug("\x1b[31m[CONS]\x1b[0m info 0x%lX\n", info); 
    ipc_send(tty_cap, info);
   
    // do {
    //     ret = ipc_send(tty_cap, &cout, sizeof(cout));
    // } while (ret == -EAGAIN);

    return SUCCESS;
}

int cons_out(const char *format, ...)
{    
    va_list va;
    va_start(va, format);
    char buffer[100];
//    memset(buffer, 0, 100);
    const int ret = vsnprintf(buffer, 100, format, va);
    console_puts(buffer);
    va_end(va);
    return ret;
}