#include <common.h>
#include <mosstd.h>
#include <string.h>
#include <libsys/cap.h>
#include <ipc.h>

#include "wm.h"


static wm_server_t WM;


static inline uint64_t wm_next_wid()
{
    return __atomic_fetch_add(&WM.next_wid, 1, __ATOMIC_ACQ_REL);
}

static inline uint64_t wm_next_z()
{
    return __atomic_fetch_add(&WM.next_z, 1, __ATOMIC_ACQ_REL);
}

static void rect_clip(int32_t *x, int32_t *y, int32_t *w, int32_t *h, int32_t clip_w, int32_t clip_h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > clip_w) *w = clip_w - *x;
    if (*y + *w > clip_h) *h = clip_h - *y;
    if (*w < 0) *w = 0;
    if (*h < 0) *h = 0;
}

static void blit_window_to_fb(wm_window_t *w, uint32_t src_x, uint32_t src_y,
                            uint32_t dst_x, uint32_t dst_y, uint32_t wdt, uint32_t hgt, 
                            void *dst_base, uint32_t dst_pitch)
{
    if (!w || !w->shm_adddr || !dst_base) return;

    uint8_t *src = (uint8_t *)w->shm_adddr;
    uint8_t *dst = (uint8_t *)dst_base;
    uint32_t src_pitch = w->width * 4;

    for (uint32_t row = 0; row < hgt; ++row) {
        uint8_t *s = src + ((size_t)(src_y + row) * src_pitch) + (src_x * 4);
        uint8_t *d = dst + ((size_t)(dst_y + row) * dst_pitch) + (dst_x * 4);
        // native copy with alfa blending: pre-multiplied alfa not assumed -> simple over operator
        for (uint32_t col = 0; col < wdt; ++col) {
            uint8_t sa = s[3];
            if (sa == 0xFF) {
                // opaque, direct copy
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
            } else if (sa == 0x00) {
                // transparent: leave destination
            } else {
                // simple alfa blend: dst = src*alfa + dst*(1-alfa)
                // shout be with float ??? FIXME
                //uint8_t a = sa;  
                float a = sa / 255.0f;
                d[0] = (uint8_t)(s[0] * a + d[0] * (1.0f - a));
                d[1] = (uint8_t)(s[1] * a + d[1] * (1.0f - a));
                d[2] = (uint8_t)(s[2] * a + d[2] * (1.0f - a));
                // d[0] = (uint8_t)(s[0] * a + d[0] * (1 - a));
                // d[1] = (uint8_t)(s[1] * a + d[1] * (1 - a));
                // d[2] = (uint8_t)(s[2] * a + d[2] * (1 - a));
                d[3] = 0xFF;
            }
            s += 4; d += 4;
        }
    }

}

void wm_composite(void)
{
    if (!WM.backbuffer) {
        size_t size = (size_t)WM.fb_pitch * WM.fb_h;
        WM.backbuffer = malloc(size);
        if (!WM.backbuffer) {
            debug("[WM] cannot allocate backbuffer\n");
            return;
        }
        WM.buf_size = size;
    }

//    int any_redraw = 0;
    // for (int i = 0; i < WM.wcount; i++) {
    //     if ()
    // }

    // if (!any_redraw) return;

    memset(WM.backbuffer, 0, (size_t)WM.buf_size);
    // background
    fb_fill_rect(&WM, 0, 0, WM.fb_w, WM.fb_h, 0x008080);

 //   foreach() {
       
        wm_window_t *w = WM.w;
     if (!w) return;    
        if (!w->visible || !w->shm_adddr) return; // continue;
        int32_t sx = 0, sy = 0;
        int32_t dx = w->x, dy = w->y;
        int32_t wdt = w->width, hgt = w->height;
        rect_clip(&dx, &dy, &wdt, &hgt, WM.fb_w, WM.fb_h);
        if (wdt == 0 || hgt == 0) return;  // continue

        sx = dx < w->x ? (w->x - dx) : 0;
        sy = dy < w->y ? (w->y - dy) : 0;

        blit_window_to_fb(w, (uint32_t)sx, (uint32_t)sy, (uint32_t)dx, (uint32_t)dy,
                         (uint32_t)wdt, (uint32_t)hgt, WM.backbuffer, WM.fb_pitch);
        fb_draw_rect(&WM, dx-3, dy-3, wdt+6, hgt+6, 0xffff, 2);                         
      //  w->needs_redraw = 0;
 //      }

    // What a shit 
    uint8_t *dst = (uint8_t *)WM.fb_base;
    uint8_t *src = (uint8_t *)WM.backbuffer;
    for (uint32_t row = 0; row < WM.fb_h; ++row) {
        memcpy(dst + (size_t)row * WM.fb_pitch, src + (size_t)row * WM.fb_pitch, WM.fb_w * 4);
    }
       cache_flush((void *)FB_BASE, (size_t)FB_SIZE);
}


int wm_create_window( uint64_t sender, wm_msg_create_t *msg, uint32_t *out_wid, int *out_shm)
{
    wm_window_t *w = malloc(sizeof(wm_window_t));
    if (!w) return -ENOMEM;

    memset(w, 0, sizeof(*w));

    w->owner = sender;
    w->id = wm_next_wid();
    w->z = wm_next_z();

    w->width = msg->width;
    w->height = msg->height;
    w->x = 16 + (WM.wcount *16) % (WM.fb_w - w->width);
    w->y = 16 + (WM.wcount *16) % (WM.fb_h - w->height);
    w->visible = 1;
    w->focused = 0;
    w->needs_redraw = 1;
    w->title[0] = '\0';

    uint64_t shm_size = PGROUNDUP((uint64_t)w->width * w->height * 4);

    // allocate shm
    int shm_cap = cap_shmem_create(shm_size, CRIGHT_GRANT | CRIGHT_MAP);
    if (shm_cap < 0) return shm_cap;

    //map shm
    void *addr = ipc_shm_attach(shm_cap, NULL, 0);
    if (!addr) return -ENOENT;

    memset(addr, 0, shm_size);

//    w->shm = shm_cap;
    w->shm_adddr = addr;
    w->shm_size  = shm_size;

    // add window to list
    WM.w = w;

    *out_wid = w->id;
    *out_shm = shm_cap;
    
    return SUCCESS;
}

int wm_init()
{
    memset(&WM, 0, sizeof(WM));

    WM.fb_base = mmap(NULL, FB_SIZE, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)FB_BASE);
    
    if (!WM.fb_base) return -EIO;

    WM.fb_w     = 1920;
    WM.fb_h     = 1080;
    WM.fb_pitch = 7680;
    WM.fb_bpp   = 32;

    WM.cursor_x = WM.fb_w >> 1;
    WM.cursor_y = WM.fb_h >> 1;

    WM.next_z = 1;
    WM.next_wid = 1;

    WM.wcount = 1;

    return SUCCESS;
}