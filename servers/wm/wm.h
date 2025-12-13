#ifndef __WM_H__
#define __WM_H__

#include <stdint.h>
#include <stddef.h>
#include <wm_msg.h>


typedef struct 
{
    uint32_t id;
    uint64_t owner;
    uint32_t x, y;
    uint32_t width, height;
    int      shm;
    void     *shm_adddr;
    uint64_t shm_size;
    char     title[64];
    uint8_t  visible;
    uint8_t  focused;
    uint8_t  needs_redraw;
    uint32_t z;
} wm_window_t;


typedef struct
{
    void     *fb_base;
    uint64_t next_wid;
    uint64_t next_z;   
    uint32_t fb_w, fb_h, fb_pitch, fb_bpp;
    uint32_t wcount;
    uint32_t cursor_x, cursor_y;
    wm_window_t *w;     // only for test. should be list
    void     *backbuffer;
    size_t   buf_size;
} wm_server_t;

int wm_init();
void wm_composite(void);
int wm_create_window( uint64_t sender, wm_msg_create_t *msg, uint32_t *out_wid, int *out_shm);

void fb_draw_rect(wm_server_t *wm, unsigned short x, unsigned short y, 
                    unsigned short w, unsigned short h, unsigned short c, unsigned short b);
void fb_fill_rect(wm_server_t *wm, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

#endif /* __WM_H__ */