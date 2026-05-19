#include <common.h>
#include <mosstd.h>
#include <string.h>

#include "wm.h"


static inline void write_pixel_32(uint8_t *pixel_prt, uint32_t color)
{
    *(uint32_t *)pixel_prt = color;
}

static inline void write_pixel_24(uint8_t *pixel_prt, uint32_t color)
{
    pixel_prt[0] = (uint8_t)((color >> 16) & 0xFF);
    pixel_prt[1] = (uint8_t)((color >> 8) & 0xFF);
    pixel_prt[2] = (uint8_t)(color & 0xFF);
}

static void fb_put_pixel(wm_server_t *wm, uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= wm->fb_w || y >= wm->fb_h || !wm->backbuffer) return;

    uint8_t *base = (uint8_t *)wm->backbuffer;
    uint8_t *row  = base + (size_t)y * wm->fb_pitch;
    uint8_t *pixel = row + (size_t)x * (wm->fb_bpp / 8);

    if (wm->fb_bpp == 32) {
        write_pixel_32(pixel, color);
    } else if (wm->fb_bpp == 24) {
        write_pixel_24(pixel, color);
    } else {
        // ??? FIXME 8bpp and 16bpp
        write_pixel_32(pixel, color);
    }
}

void fb_fill_rect(wm_server_t *wm, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t row = 0; row < h; ++row) {
        uint8_t *line = (uint8_t *)wm->backbuffer + (y + row) * wm->fb_pitch + x * (wm->fb_bpp / 8);
        if (wm->fb_bpp == 32) {
            uint32_t *p = (uint32_t *)line;
            for (uint32_t col = 0; col < w; ++col) {
                p[col] = color;
            }
        } else if (wm->fb_bpp == 24) {
            for (uint32_t col = 0; col < w; ++col) {
                uint8_t *pixel = line + col * 3;
                pixel[0] = (uint8_t)((color >> 16) & 0xFF);
                pixel[1] = (uint8_t)((color >> 8) & 0xFF);
                pixel[2] = (uint8_t)(color & 0xFF);
            }
        } else {
            uint32_t *p = (uint32_t *)line;
            for (uint32_t col = 0; col < w; ++col) 
                p[col] = color;
        }
    }
}


// This verson creates rectangles with border widths.
void fb_draw_rect(wm_server_t *wm, unsigned short x, unsigned short y, 
                    unsigned short w, unsigned short h, unsigned short c, unsigned short b) 
{
    unsigned short cx, cy;
    for (cy = 0; cy < h; cy++) {
        for (cx = 0; cx < w; cx++) {
			if (((cx < b) || ((w-cx) <= b))  || 
				 ((cy < b) || ((h-cy) <= b))){
				fb_put_pixel(wm, x + cx, y + cy, c);
			} // if
		}
    }
}


// static void fb_draw_line(wm_server_t *wm, int x0, int y0, int x1, int y1, uint32_t c) 
// {
//     int dx, dy, p, x, y;
// 	dx=x1-x0;
// 	dy=y1-y0;
// 	x=x0;
// 	y=y0;
// 	p=2*dy-dx;
// 	while(x<x1) {
// 		if(p>=0){ 
// 			fb_put_pixel(wm, x,y,c);
// 			y=y+1;
// 			p=p+2*dy-2*dx;
// 		} else {
// 			fb_put_pixel(wm, x,y,c);
// 			p=p+2*dy;
// 		} // if
// 		x=x+1;
// 	}
// }