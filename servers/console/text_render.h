#ifndef __TEXT_RENDER_H__
#define __TEXT_RENDER_H__

#include <stdint.h>
#include <stddef.h>

#define PSF1_MAGIC 0x0436

#define PSF2_MAGIC 0x864ab572

/*
    PSF1
*/
typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t fontmode; // PSF font mode.
    uint8_t charsize; // PSF character size.
} psf1_header_t;

/*
    PSF2
*/
typedef struct {
    uint32_t magic;         /* magic bytes to identify PSF */
    uint32_t version;       /* zero */
    uint32_t headersize;    /* offset of bitmaps in file, 32 */
    uint32_t flags;         /* 0 if there's no unicode table */
    uint32_t numglyph;      /* number of glyphs */
    uint32_t bytesperglyph; /* size of each glyph */
    uint32_t height;        /* height in pixels */
    uint32_t width;         /* width in pixels */
} psf2_header_t;

typedef enum {
    FONT_PSF1 = 1,
    FONT_PSF2,
} font_type_t;

typedef struct {
    font_type_t type;
    union {
        psf1_header_t *psf1;
        psf2_header_t *psf2;
    };
    uint32_t char_w;
    uint32_t char_h;
    uint32_t bytes_per_glyph;
    uint32_t glyphs;
    uint8_t *glyph_buf;
} font_t;

typedef struct
{
    uint32_t *base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;   
    uint32_t cols;
    uint32_t rows;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t fg_color;
    uint32_t bg_color;
    font_t font;
} fb_console_t;

int font_load(void *data, font_t *out);
int fb_console_init(fb_console_t *c, void *fb_base, uint32_t width, uint32_t height,
                    uint32_t bpp, uint32_t pitch, uint32_t fg_color, uint32_t bg_color, font_t font);

void fb_puts(fb_console_t *c, const char *s);
#endif /* __TEXT_RENDER_H__ */