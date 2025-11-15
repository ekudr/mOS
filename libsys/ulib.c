#include <stdint.h>
//#include <syscall.h>
#include <libsys/ipc.h>

ipc_buffer_t *__ipc_buffer;

/*
// Print string
void
lib_puts(char *s) {  
    while (*s) {
        putc(*s++);
    }
}
*/
void
_putchar(char s) {  
//    putc(s);

}
