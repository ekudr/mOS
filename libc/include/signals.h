#ifndef __SIGNALS_H__
#define __SIGNALS_H__

#include <stdint.h>
#include <errno.h>

typedef uint32_t signal_t;
typedef uint64_t signal_payload_t;
typedef void*    signal_handler_t;

enum {
    SIGNAL_IRQ = 1,
    SIGNAL_TIMER,
    SIGNAL_SHM_READY,
    SIGNAL_CONSOLE,

    SIGNAL_USER_BASE = 0x30,   // user-defined start
};


#define MAX_NR_SIGNAL 64


typedef struct signal_action
{
    signal_handler_t handler;   // Handler 
    signal_handler_t restorer;  // Restorer  
} signal_action_t;




kerrno_t signal_action(signal_t sig, signal_action_t *sa);

void __restore(void);

#endif /* __SIGNALS_H__ */