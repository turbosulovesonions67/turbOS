#ifndef TURBOS_GFX_H
#define TURBOS_GFX_H

#include <stdint.h>

#define GFX_MAX_W 1280
#define GFX_MAX_H 1024

#define GFX_RGB(r, g, b) \
    ((uint32_t)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

#define GFX_CURSOR_W 12
#define GFX_CURSOR_H 19

int gfx_init_multiboot(unsigned int magic, unsigned int info_addr);

int gfx_init_buffer(void *fb, int pitch, int width, int height, int bpp,
                    int rpos, int rsize, int gpos, int gsize,
                    int bpos, int bsize);

int gfx_ready(void);
int gfx_width(void);
int gfx_height(void);

void gfx_clip_set(int x, int y, int w, int h);
void gfx_clip_reset(void);
void gfx_clip_get(int *x0, int *y0, int *x1, int *y1);

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_rect_outline(int x, int y, int w, int h, uint32_t color);
void gfx_hline(int x, int y, int w, uint32_t color);
void gfx_vline(int x, int y, int h, uint32_t color);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_fill_circle(int cx, int cy, int r, uint32_t color);
void gfx_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha);

void gfx_draw_cell(int x, int y, char ch, uint32_t fg, uint32_t bg);
void gfx_draw_char(int x, int y, char ch, uint32_t fg);
void gfx_draw_text(int x, int y, const char *s, uint32_t fg);
int  gfx_text_width(const char *s);

void gfx_present(int x, int y, int w, int h);

void gfx_fb_clear(uint32_t color);
void gfx_fb_draw_cell(int x, int y, char ch, uint32_t fg, uint32_t bg);

void gfx_cursor_draw(int x, int y);
void gfx_cursor_erase(int x, int y);

const uint32_t *gfx_scene(void);

#endif
