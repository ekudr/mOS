#include <common.h>
#include <string.h>

#include "text_render.h"

static inline int is_pfs1(psf1_header_t *h)
{
    return (h->magic == PSF1_MAGIC);
}

static inline int is_pfs2(psf2_header_t *h)
{
    return (h->magic == PSF2_MAGIC);
}

int font_load(void *data, font_t *out)
{
    psf1_header_t *h1 = (psf1_header_t *)data;

    if (is_pfs1(h1)) {
        out->type = FONT_PSF1;
        out->psf1 = h1;
        out->glyph_buf = (uint8_t *)data + sizeof(psf1_header_t);
        out->char_h = h1->charsize;
        out->char_w = 8;
        out->bytes_per_glyph = h1->charsize;
        out->glyphs = (h1->fontmode & 0x01) ? 512U : 256U;
        return SUCCESS;
    }

    psf2_header_t *h2 = (psf2_header_t *)data;
    if (is_pfs2(h2)) {
        out->type = FONT_PSF2;
        out->psf2 = h2;
        out->glyph_buf = (uint8_t *)data + h2->headersize;
        out->char_h = h2->height;
        out->char_w = h2->width;
        out->bytes_per_glyph = h2->bytesperglyph;
        out->glyphs = h2->numglyph;
        return SUCCESS;
    }
    
    return -ENOSUPPORT;
}

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

static void fb_put_pixel(fb_console_t *c, uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= c->width || y >= c->height) return;

    uint8_t *base = (uint8_t *)c->base;
    uint8_t *row  = base + (size_t)y * c->pitch;
    uint8_t *pixel = row + (size_t)x * (c->bpp / 8);

    if (c->bpp == 32) {
        write_pixel_32(pixel, color);
    } else if (c->bpp == 24) {
        write_pixel_24(pixel, color);
    } else {
        // ??? FIXME 8bpp and 16bpp
        write_pixel_32(pixel, color);
    }
}

static void fb_fill_rect(fb_console_t *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t row = 0; row < h; ++row) {
        uint8_t *line = (uint8_t *)c->base + (y + row) * c->pitch + x * (c->bpp / 8);
        if (c->bpp == 32) {
            uint32_t *p = (uint32_t *)line;
            for (uint32_t col = 0; col < w; ++col) {
                p[col] = color;
            }
        } else if (c->bpp == 24) {
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

int fb_console_init(fb_console_t *c, void *fb_base, uint32_t width, uint32_t height,
                    uint32_t bpp, uint32_t pitch, uint32_t fg_color, uint32_t bg_color, font_t font)
{
    if (!c || !fb_base) return -EINVAL;

    c->base   = (uint32_t *)fb_base;
    c->width  = width;
    c->height = height;
    c->bpp    = bpp;
    c->pitch  = pitch;

    c->fg_color = fg_color;
    c->bg_color = bg_color;

    c->cursor_x = 0;
    c->cursor_y = 0;

    c->font = font;

    c->cols = width / font.char_w;
    c->rows = height / font.char_h;
//    debug("[CONS] char width %d char height %d\n", font.char_w, font.char_h);
//    debug("[CONS] width %d height %d\n", c->cols, c->rows);
    debug("[CONS] num glyphs %d\n", c->font.glyphs);
    fb_fill_rect(c, 0, 0, c->width, c->height, c->bg_color);
    cash_flash((void *)FB_BASE, (size_t)FB_SIZE);

    return SUCCESS;
}

static void fb_console_scrollup(fb_console_t *c)
{
    uint32_t char_h = c->font.char_h;
    size_t row_bytes = (size_t)char_h * c->pitch;
    uint8_t *base = (uint8_t *)c->base;

    memmove((void *)base, (void *)base + row_bytes, (size_t)(c->height * c->pitch) - row_bytes);

    fb_fill_rect(c, 0, c->height - char_h, c->width, char_h, c->bg_color);

    if (c->cursor_y > 0) c->cursor_y--;
}

void fb_draw_glyph(fb_console_t *c, uint32_t code, uint32_t x, uint32_t y)
{
    font_t *f = &c->font;

    if (code >= f->glyphs) {
        code = '?';
    }

    uint32_t px_x = x * f->char_w;
    uint32_t px_y = y * f->char_h;

    uint8_t *glyph = f->glyph_buf + (size_t)code * f->bytes_per_glyph;

    for (uint32_t row = 0; row < f->char_h; row++) {
        uint32_t row_bytes_offset = row * ((f->char_w + 7) / 8);
        for (uint32_t col = 0; col < f->char_w; col++) {
            uint8_t byte = glyph[row_bytes_offset + col / 8];
            uint8_t bit = 0x80 >> (col % 8);
            uint32_t color = (byte & bit) ? c->fg_color : c->bg_color;
            fb_put_pixel(c, px_x + col, px_y + row, color);
        }
    }


 }

static void fb_new_line(fb_console_t *c)
{
    c->cursor_x = 0;
    c->cursor_y++;
    if (c->cursor_y >= c->rows) {
        fb_console_scrollup(c);
        c->cursor_y = c->rows - 1;
    }
}

static void fb_draw_cursor(fb_console_t *c)
{
    uint32_t px_y = c->cursor_y * c->font.char_h;
    uint32_t px_x = c->cursor_x * c->font.char_w;

    uint32_t cy = px_y + c->font.char_h - 2;

    for (uint32_t x = 0; x < c->font.char_w; x++) {
        fb_put_pixel(c, px_x + x, cy, c->fg_color);
    }
}

static inline void fb_erase_cursor(fb_console_t *c)
{
    fb_draw_glyph(c, ' ', c->cursor_x, c->cursor_y);
}

void fb_putc(fb_console_t *c, unsigned char ch)
{
    if (!c) return;

//    fb_erase_cursor(c);

    switch (ch)
    {
    case '\r':
        fb_erase_cursor(c);
        c->cursor_x = 0;
        break;

    case '\n':
        fb_erase_cursor(c);
        fb_new_line(c);
        break;

    case '\t':
        fb_erase_cursor(c);
        uint32_t tab_width = 4;
        uint32_t next = (c->cursor_x / tab_width + 1) * tab_width;
        if (next >= c->cols) {
            fb_new_line(c);
        } else {
            c->cursor_x = next;
        }
        break;

    case '\b':  // backspace
        fb_erase_cursor(c);
        if (c->cursor_x > 0) {
            c->cursor_x--;
            fb_draw_glyph(c, ' ', c->cursor_x, c->cursor_y);
        }
        break;
    
    default:
        fb_draw_glyph(c, ch, c->cursor_x, c->cursor_y);
        c->cursor_x++;
        if (c->cursor_x >= c->cols) {
            fb_new_line(c);
        }
        break;
    }
    fb_draw_cursor(c);
//    fb_draw_glyph(c, '_', c->cursor_x+1, c->cursor_y+1);
}

void fb_puts(fb_console_t *c, const char *s)
{
    if (!c || !s) return;

    while (*s)
    {
        fb_putc(c, (unsigned char)*s++);
    }
    cash_flash((void *)FB_BASE, (size_t)FB_SIZE);
}

void fb_set_colors(fb_console_t *c, uint32_t fg, uint32_t bg)
{
    if (!c) return;

    c->fg_color = fg;
    c->bg_color = bg;
}

void fb_clear(fb_console_t *c)
{
    if (!c) return;

    fb_fill_rect(c, 0, 0, c->width, c->height, c->bg_color);
    c->cursor_x = 0;
    c->cursor_y = 0;
}

void fb_set_cursor(fb_console_t *c, uint32_t x, uint32_t y)
{
    if (!c) return;

    if (x >= c->cols) x = c->cols - 1;
    if (y >= c->rows) x = c->rows - 1;

    c->cursor_x = x;
    c->cursor_y = y;
}

