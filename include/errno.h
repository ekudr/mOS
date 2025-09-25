#ifndef __ERRNO_H__
#define __ERRNO_H__

typedef enum {
    SUCCESS = 0,
    ENOPERM,
    EINVAL,
    EAGAIN,     /* Try again */
    ENOMEM,
    EIO,
    EEXIST,
    ENOSPC,     /* No Space */
    ENOENT,     /* No Entry, not Exist */
    ENOSUPPORT,
    ENOALIGN,
} kerrno_t;



#endif /* __ERRNO_H__ */