#ifndef __WM_MSG_H__
#define __WM_MSG_H__

#include <stdint.h>

enum {
    WM_CMD_CREATE_WINDOW = 1,
    WM_CMD_DESTROY_WINDOW,
};

enum {
    WM_EVT_INVAL_INPUT = 1,
    WM_EVT_WINDOW_CREATED,
    WM_EVT_WINDOW_DESTROYED,
};

typedef struct 
{
    uint32_t type;
    uint32_t length;
} __attribute__((packed)) wm_msg_hdr_t;

typedef struct 
{
    uint32_t width;
    uint32_t height;
    uint32_t flags;
} __attribute__((packed)) wm_msg_create_t;

#endif /* __WM_MSG_H__ */