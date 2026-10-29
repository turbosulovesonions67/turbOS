#include "gui/gfx.h"
#include "gui/font8x16.h"

static uint32_t scene[GFX_MAX_W * GFX_MAX_H];

static uint8_t *fb_base;
static int fb_pitch;
static int g_w;
static int g_h;
static int g_ready;

static int native32;
static int rpos, rsize, gpos, gsize, bpos, bsize;

static int clip_x0, clip_y0, clip_x1, clip_y1;

static uint32_t pack(uint32_t c)
{
    uint32_t r, g, b;

    if(native32)
        return c & 0x00FFFFFFu;

    r = (c >> 16) & 0xFFu;
    g = (c >> 8) & 0xFFu;
    b = c & 0xFFu;

    return ((r >> (8 - rsize)) << rpos) |
           ((g >> (8 - gsize)) << gpos) |
           ((b >> (8 - bsize)) << bpos);
}

static uint32_t *fb_row(int y)
{
    return (uint32_t *)(fb_base + (unsigned int)y * (unsigned int)fb_pitch);
}

static void copy32(uint32_t *dst, const uint32_t *src, int n)
{
#if defined(__i386__)
    __asm__ volatile("cld; rep movsl"
                     : "+D"(dst), "+S"(src), "+c"(n)
                     :
                     : "memory", "cc");
#else
    int i;

    for(i = 0; i < n; i++)
        dst[i] = src[i];
#endif
}

static const unsigned char *glyph_for(char ch)
{
    unsigned char c = (unsigned char)ch;

    if(c == 0)
        c = ' ';
    else if(c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR)
        c = '?';

    return font8x16[c - FONT_FIRST_CHAR];
}

static void put_px(int x, int y, uint32_t c)
{
    if(x < clip_x0 || y < clip_y0 || x >= clip_x1 || y >= clip_y1)
        return;

    scene[y * g_w + x] = c;
}

static int valid_field(int pos, int size)
{
    return size >= 1 && size <= 8 && pos >= 0 && pos + size <= 32;
}

int gfx_init_buffer(void *fb, int pitch, int width, int height, int bpp,
                    int r_pos, int r_size, int g_pos, int g_size,
                    int b_pos, int b_size)
{
    int i;

    g_ready = 0;

    if(!fb || bpp != 32 || width < 320 || height < 200)
        return 0;

    if(pitch < width * 4)
        return 0;

    if(!valid_field(r_pos, r_size) ||
       !valid_field(g_pos, g_size) ||
       !valid_field(b_pos, b_size))
        return 0;

    fb_base = (uint8_t *)fb;
    fb_pitch = pitch;

    g_w = width > GFX_MAX_W ? GFX_MAX_W : width;
    g_h = height > GFX_MAX_H ? GFX_MAX_H : height;

    rpos = r_pos;
    rsize = r_size;
    gpos = g_pos;
    gsize = g_size;
    bpos = b_pos;
    bsize = b_size;

    native32 = (rpos == 16 && rsize == 8 &&
                gpos == 8 && gsize == 8 &&
                bpos == 0 && bsize == 8);

    for(i = 0; i < g_w * g_h; i++)
        scene[i] = 0;

    g_ready = 1;

    gfx_clip_reset();
    gfx_fb_clear(0);

    return 1;
}

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

int gfx_init_multiboot(unsigned int magic, unsigned int info_addr)
{
    const unsigned char *base;
    unsigned int total;
    unsigned int off;

    if(magic != 0x36d76289U || !info_addr)
        return 0;

    base = (const unsigned char *)info_addr;
    total = rd32(base);

    if(total < 16)
        return 0;

    off = 8;

    while(off + 8 <= total)
    {
        const unsigned char *tag = base + off;
        unsigned int type = rd32(tag);
        unsigned int size = rd32(tag + 4);

        if(size < 8 || size > total - off)
            return 0;

        if(type == 0)
            break;

        if(type == 8)
        {
            unsigned int addr_lo;
            unsigned int addr_hi;
            unsigned int pitch;
            unsigned int width;
            unsigned int height;

            if(size < 38 || tag[29] != 1)
                return 0;

            addr_lo = rd32(tag + 8);
            addr_hi = rd32(tag + 12);
            pitch = rd32(tag + 16);
            width = rd32(tag + 20);
            height = rd32(tag + 24);

            if(addr_hi != 0 || width > 16384 || height > 16384 ||
               pitch > 0x100000)
                return 0;

            return gfx_init_buffer((void *)addr_lo,
                                   (int)pitch, (int)width, (int)height,
                                   tag[28],
                                   tag[32], tag[33],
                                   tag[34], tag[35],
                                   tag[36], tag[37]);
        }

        off = (off + size + 7U) & ~7U;
    }

    return 0;
}

int gfx_ready(void)
{
    return g_ready;
}

int gfx_width(void)
{
    return g_w;
}

int gfx_height(void)
{
    return g_h;
}

const uint32_t *gfx_scene(void)
{
    return scene;
}

void gfx_clip_set(int x, int y, int w, int h)
{
    int x1 = x + w;
    int y1 = y + h;

    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;
    if(x1 > g_w)
        x1 = g_w;
    if(y1 > g_h)
        y1 = g_h;

    if(x1 < x)
        x1 = x;
    if(y1 < y)
        y1 = y;

    clip_x0 = x;
    clip_y0 = y;
    clip_x1 = x1;
    clip_y1 = y1;
}

void gfx_clip_reset(void)
{
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = g_w;
    clip_y1 = g_h;
}

void gfx_clip_get(int *x0, int *y0, int *x1, int *y1)
{
    *x0 = clip_x0;
    *y0 = clip_y0;
    *x1 = clip_x1;
    *y1 = clip_y1;
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    int x1 = x + w;
    int y1 = y + h;
    int xx;
    int yy;

    if(x < clip_x0)
        x = clip_x0;
    if(y < clip_y0)
        y = clip_y0;
    if(x1 > clip_x1)
        x1 = clip_x1;
    if(y1 > clip_y1)
        y1 = clip_y1;

    if(x >= x1 || y >= y1)
        return;

    for(yy = y; yy < y1; yy++)
    {
        uint32_t *p = scene + yy * g_w + x;

        for(xx = x; xx < x1; xx++)
            *p++ = color;
    }
}

void gfx_hline(int x, int y, int w, uint32_t color)
{
    gfx_fill_rect(x, y, w, 1, color);
}

void gfx_vline(int x, int y, int h, uint32_t color)
{
    gfx_fill_rect(x, y, 1, h, color);
}

void gfx_rect_outline(int x, int y, int w, int h, uint32_t color)
{
    if(w <= 0 || h <= 0)
        return;

    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

void gfx_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for(;;)
    {
        int e2;

        put_px(x0, y0, color);

        if(x0 == x1 && y0 == y1)
            break;

        e2 = 2 * err;

        if(e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if(e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint32_t color)
{
    int x;
    int y;

    if(r <= 0)
        return;

    for(y = -r; y <= r; y++)
        for(x = -r; x <= r; x++)
            if(x * x + y * y <= r * r + r)
                put_px(cx + x, cy + y, color);
}

void gfx_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha)
{
    int x1 = x + w;
    int y1 = y + h;
    int xx;
    int yy;
    int sr;
    int sg;
    int sb;

    if(alpha <= 0)
        return;
    if(alpha > 255)
        alpha = 255;

    if(x < clip_x0)
        x = clip_x0;
    if(y < clip_y0)
        y = clip_y0;
    if(x1 > clip_x1)
        x1 = clip_x1;
    if(y1 > clip_y1)
        y1 = clip_y1;

    if(x >= x1 || y >= y1)
        return;

    sr = (int)((color >> 16) & 0xFF);
    sg = (int)((color >> 8) & 0xFF);
    sb = (int)(color & 0xFF);

    for(yy = y; yy < y1; yy++)
    {
        uint32_t *p = scene + yy * g_w + x;

        for(xx = x; xx < x1; xx++, p++)
        {
            int dr = (int)((*p >> 16) & 0xFF);
            int dg = (int)((*p >> 8) & 0xFF);
            int db = (int)(*p & 0xFF);

            dr = (sr * alpha + dr * (255 - alpha) + 127) / 255;
            dg = (sg * alpha + dg * (255 - alpha) + 127) / 255;
            db = (sb * alpha + db * (255 - alpha) + 127) / 255;

            *p = ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | (uint32_t)db;
        }
    }
}

void gfx_draw_cell(int x, int y, char ch, uint32_t fg, uint32_t bg)
{
    const unsigned char *g = glyph_for(ch);
    int row;
    int col;

    if(x >= clip_x0 && y >= clip_y0 &&
       x + FONT_W <= clip_x1 && y + FONT_H <= clip_y1)
    {
        for(row = 0; row < FONT_H; row++)
        {
            uint32_t *p = scene + (y + row) * g_w + x;
            unsigned int bits = g[row];

            p[0] = (bits & 0x80) ? fg : bg;
            p[1] = (bits & 0x40) ? fg : bg;
            p[2] = (bits & 0x20) ? fg : bg;
            p[3] = (bits & 0x10) ? fg : bg;
            p[4] = (bits & 0x08) ? fg : bg;
            p[5] = (bits & 0x04) ? fg : bg;
            p[6] = (bits & 0x02) ? fg : bg;
            p[7] = (bits & 0x01) ? fg : bg;
        }

        return;
    }

    for(row = 0; row < FONT_H; row++)
        for(col = 0; col < FONT_W; col++)
            put_px(x + col, y + row,
                   (g[row] & (0x80 >> col)) ? fg : bg);
}

void gfx_draw_char(int x, int y, char ch, uint32_t fg)
{
    const unsigned char *g = glyph_for(ch);
    int row;
    int col;

    for(row = 0; row < FONT_H; row++)
    {
        unsigned int bits = g[row];

        if(!bits)
            continue;

        for(col = 0; col < FONT_W; col++)
            if(bits & (0x80u >> col))
                put_px(x + col, y + row, fg);
    }
}

void gfx_draw_text(int x, int y, const char *s, uint32_t fg)
{
    while(*s)
    {
        gfx_draw_char(x, y, *s, fg);
        x += FONT_W;
        s++;
    }
}

int gfx_text_width(const char *s)
{
    int n = 0;

    while(s[n])
        n++;

    return n * FONT_W;
}

void gfx_present(int x, int y, int w, int h)
{
    int x1 = x + w;
    int y1 = y + h;
    int row;

    if(!g_ready)
        return;

    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;
    if(x1 > g_w)
        x1 = g_w;
    if(y1 > g_h)
        y1 = g_h;

    if(x >= x1 || y >= y1)
        return;

    for(row = y; row < y1; row++)
    {
        uint32_t *dst = fb_row(row) + x;
        const uint32_t *src = scene + row * g_w + x;
        int n = x1 - x;

        if(native32)
        {
            copy32(dst, src, n);
        }
        else
        {
            int i;

            for(i = 0; i < n; i++)
                dst[i] = pack(src[i]);
        }
    }
}

void gfx_fb_clear(uint32_t color)
{
    uint32_t packed;
    int x;
    int y;

    if(!g_ready)
        return;

    packed = pack(color);

    for(y = 0; y < g_h; y++)
    {
        uint32_t *p = fb_row(y);

        for(x = 0; x < g_w; x++)
            p[x] = packed;
    }
}

void gfx_fb_draw_cell(int x, int y, char ch, uint32_t fg, uint32_t bg)
{
    const unsigned char *g;
    uint32_t pf;
    uint32_t pb;
    int row;

    if(!g_ready || x < 0 || y < 0 ||
       x + FONT_W > g_w || y + FONT_H > g_h)
        return;

    g = glyph_for(ch);
    pf = pack(fg);
    pb = pack(bg);

    for(row = 0; row < FONT_H; row++)
    {
        uint32_t *p = fb_row(y + row) + x;
        unsigned int bits = g[row];

        p[0] = (bits & 0x80) ? pf : pb;
        p[1] = (bits & 0x40) ? pf : pb;
        p[2] = (bits & 0x20) ? pf : pb;
        p[3] = (bits & 0x10) ? pf : pb;
        p[4] = (bits & 0x08) ? pf : pb;
        p[5] = (bits & 0x04) ? pf : pb;
        p[6] = (bits & 0x02) ? pf : pb;
        p[7] = (bits & 0x01) ? pf : pb;
    }
}

static const char cursor_shape[GFX_CURSOR_H][GFX_CURSOR_W + 1] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXX ",
    "X..X..X     ",
    "X.X X..X    ",
    "XX  X..X    ",
    "X    X..X   ",
    "     X..X   ",
    "      X..X  ",
    "      X..X  ",
    "       XX   "
};

void gfx_cursor_draw(int x, int y)
{
    uint32_t black;
    uint32_t white;
    int row;
    int col;

    if(!g_ready)
        return;

    black = pack(GFX_RGB(0, 0, 0));
    white = pack(GFX_RGB(255, 255, 255));

    for(row = 0; row < GFX_CURSOR_H; row++)
    {
        int py = y + row;

        if(py < 0 || py >= g_h)
            continue;

        for(col = 0; col < GFX_CURSOR_W; col++)
        {
            char s = cursor_shape[row][col];
            int px = x + col;

            if(s == ' ' || px < 0 || px >= g_w)
                continue;

            fb_row(py)[px] = (s == 'X') ? black : white;
        }
    }
}

void gfx_cursor_erase(int x, int y)
{
    gfx_present(x, y, GFX_CURSOR_W, GFX_CURSOR_H);
}
