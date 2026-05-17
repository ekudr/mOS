#ifndef __LIBSYS_IPC_H__
#define __LIBSYS_IPC_H__

#include <stdint.h>
#include <syscall.h>

#ifndef IPC_MAX_MSG_LEN
#define IPC_MAX_MSG_LEN 500
#endif /* IPC_MAX_MSG_LEN */

#ifndef IPC_MAX_CAPS
#define IPC_MAX_CAPS 4
#endif /* IPC_MAX_MSG_LEN */


#define MSGINFO_LABEL_BITS    32
#define MSGINFO_LABEL_SHIFT   32
#define MSGINFO_LABEL_MASK    ((1ULL << MSGINFO_LABEL_BITS)-1)

#define MSGINFO_LEN_BITS    16
#define MSGINFO_LEN_SHIFT   16
#define MSGINFO_LEN_MASK    ((1ULL << MSGINFO_LEN_BITS)-1)

#define MSGINFO_XCAPS_BITS    8
#define MSGINFO_XCAPS_SHIFT   8
#define MSGINFO_XCAPS_MASK    ((1ULL << MSGINFO_XCAPS_BITS)-1)

#define MSGINFO_FLAGS_BITS    8
#define MSGINFO_FLAGS_SHIFT   0
#define MSGINFO_FLAGS_MASK    ((1ULL << MSGINFO_FLAGS_BITS)-1)

#define MSGINFO_NOTIFICATION  0x01  // flag: delivery is from a notification, not IPC

typedef uint64_t msg_info_t;

#define label_from_msginfo_word(w) (((msg_info_t)w >> MSGINFO_LABEL_SHIFT) & MSGINFO_LABEL_MASK) 
#define length_from_msginfo_word(w) (((msg_info_t)w >> MSGINFO_LEN_SHIFT) & MSGINFO_LEN_MASK) 
#define xcaps_from_msginfo_word(w) (((uint64_t)w >> MSGINFO_XCAPS_SHIFT) & MSGINFO_XCAPS_MASK)

inline msg_info_t msginfo_word_new(uint32_t label, uint16_t length, uint8_t caps, uint8_t flags)
{
    uint64_t info = 0;
    info = (label & MSGINFO_LABEL_MASK) << MSGINFO_LABEL_SHIFT; 
    info |= (length & MSGINFO_LEN_MASK) << MSGINFO_LEN_SHIFT;
    info |= (caps & MSGINFO_XCAPS_MASK) << MSGINFO_XCAPS_SHIFT;
    info |= (flags & MSGINFO_FLAGS_MASK);
    return info;
}

typedef struct ipc_buffer
{
    uint64_t    info;
    uint64_t    msg[IPC_MAX_MSG_LEN];
    uint64_t    user_data;
    uint64_t    caps[IPC_MAX_CAPS];
} ipc_buffer_t;

_Static_assert((sizeof(struct ipc_buffer) < 0x1000), "ipc_buffers structure size");

extern ipc_buffer_t *__ipc_buffer;  

inline ipc_buffer_t *get_ipc_buffer(void)
{
    return __ipc_buffer;
}

inline uint64_t ipc_getMR(int i)
{
    return get_ipc_buffer()->msg[i];
}

inline void ipc_setMR(int i, uint64_t mr)
{
    get_ipc_buffer()->msg[i] = mr;
}

inline uint64_t ipc_get_cap(int i)
{
    return get_ipc_buffer()->caps[i];
}

inline void ipc_set_cap(int i, uint64_t mr)
{
    get_ipc_buffer()->caps[i] = mr;
}

inline void syscall_recv(uint64_t sys, uint64_t src, uint64_t *out_badge, msg_info_t *out_info, uint64_t 
                                  *out_mr0, uint64_t *out_mr1, uint64_t *out_mr2, uint64_t *out_mr3, uint64_t *out_mr4)
{
    register uint64_t src_and_badge asm("a0") = src;
    register msg_info_t info asm("a1");

    /* Incoming message registers. */
    register uint64_t msg0 asm("a2");
    register uint64_t msg1 asm("a3");
    register uint64_t msg2 asm("a4");
    register uint64_t msg3 asm("a5");
    register uint64_t msg4 asm("a6");

    /* Perform the system call. */
    register uint64_t scno asm("a7") = sys;
    asm volatile(
        "ecall"
        : "=r"(msg0), "=r"(msg1), "=r"(msg2), "=r"(msg3), "=r"(msg4),
        "=r"(info), "+r"(src_and_badge)
        : "r"(scno)
        : "memory"
    );
    *out_badge = src_and_badge;
    *out_info = info;
    *out_mr0 = msg0;
    *out_mr1 = msg1;
    *out_mr2 = msg2;
    *out_mr3 = msg3;
    *out_mr4 = msg4;
}

inline void syscall_send(uint64_t sys, uint64_t dest, msg_info_t info_arg, uint64_t mr0, uint64_t mr1,
                                  uint64_t mr2, uint64_t mr3, uint64_t mr4)
{
    register uint64_t destptr asm("a0") = dest;
    register msg_info_t info asm("a1") = info_arg;

    /* Load beginning of the message into registers. */
    register uint64_t msg0 asm("a2") = mr0;
    register uint64_t msg1 asm("a3") = mr1;
    register uint64_t msg2 asm("a4") = mr2;
    register uint64_t msg3 asm("a5") = mr3;
    register uint64_t msg4 asm("a6") = mr4;

    /* Perform the system call. */
    register uint64_t scno asm("a7") = sys;
    asm volatile(
        "ecall"
        : "+r"(destptr), "+r"(msg0), "+r"(msg1), "+r"(msg2),
        "+r"(msg3), "+r"(msg4), "+r"(info)
        : "r"(scno)
    );
}

inline void syscall_send_recv(uint64_t sys, uint64_t dest, uint64_t *out_badge, msg_info_t info_arg,
                                       msg_info_t *out_info, uint64_t *in_out_mr0, uint64_t *in_out_mr1,
                                       uint64_t *in_out_mr2, uint64_t *in_out_mr3, uint64_t *in_out_mr4)
{
    register uint64_t destptr asm("a0") = dest;
    register msg_info_t info asm("a1") = info_arg;

    /* Load beginning of the message into registers. */
    register uint64_t msg0 asm("a2") = *in_out_mr0;
    register uint64_t msg1 asm("a3") = *in_out_mr1;
    register uint64_t msg2 asm("a4") = *in_out_mr2;
    register uint64_t msg3 asm("a5") = *in_out_mr3;
    register uint64_t msg4 asm("a6") = *in_out_mr4;

    /* Perform the system call. */
    register uint64_t scno asm("a7") = sys;
    asm volatile(
        "ecall"
        : "+r"(msg0), "+r"(msg1), "+r"(msg2), "+r"(msg3), "+r"(msg4),
        "+r"(info), "+r"(destptr)
        : "r"(scno)
        : "memory"
    );
    *out_info = info;
    *out_badge = destptr;
    *in_out_mr0 = msg0;
    *in_out_mr1 = msg1;
    *in_out_mr2 = msg2;
    *in_out_mr3 = msg3;
    *in_out_mr4 = msg4;
}

inline void syscall_reply(uint64_t sys, msg_info_t info_arg, uint64_t mr0, uint64_t mr1, uint64_t mr2,
                                   uint64_t mr3, uint64_t mr4)
{
    register msg_info_t info asm("a1") = info_arg;

    /* Load beginning of the message into registers. */
    register uint64_t msg0 asm("a2") = mr0;
    register uint64_t msg1 asm("a3") = mr1;
    register uint64_t msg2 asm("a4") = mr2;
    register uint64_t msg3 asm("a5") = mr3;
    register uint64_t msg4 asm("a6") = mr4;

    /* Perform the system call. */
    register uint64_t scno asm("a7") = sys;
    asm volatile(
        "ecall"
        : "+r"(msg0), "+r"(msg1), "+r"(msg2), "+r"(msg3), "+r"(msg4),
        "+r"(info)
        : "r"(scno)
    );
}

uint64_t ipc_recv(uint64_t ep, uint64_t *sender);
uint64_t ipc_nb_recv(uint64_t src, uint64_t *sender);
void ipc_send(uint64_t dest, msg_info_t info);
void ipc_nb_send(uint64_t dest, msg_info_t info);
uint64_t ipc_call(uint64_t dest, msg_info_t info);
void ipc_reply(msg_info_t info);

// Notification API
int  notif_create(uint32_t rights);
void notif_signal(uint64_t notif_cap);
int  notif_bind(uint64_t notif_cap);
void notif_unbind(void);
uint64_t notif_wait(uint64_t notif_cap);
int  notif_poll(uint64_t notif_cap, uint64_t *word_out);
int  irq_bind_notif(uint64_t irq, uint64_t notif_cap);

static inline int msginfo_is_notification(msg_info_t info)
{
    return (info & MSGINFO_FLAGS_MASK) & MSGINFO_NOTIFICATION;
}

#endif /* __LIBSYS_IPC_H__ */