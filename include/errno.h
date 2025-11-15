#ifndef __ERRNO_H__
#define __ERRNO_H__

typedef enum {
    SUCCESS = 0,
    ENOPERM,
    EPERM,      /* Operation not permitted */
    ERR_CAP_INVAL,  /* Invalid cap*/
    EINVAL,
    EAGAIN,     /* Try again */
    ENOMEM,
    EIO,
    ETIMEOUT,
    EEXIST,
    ENOSPC,     /* No Space */
    ENOENT,     /* No Entry, not Exist */
    ENOSUPPORT,
    ENOALIGN,
    EBUSY,
} kerrno_t;



#endif /* __ERRNO_H__ */