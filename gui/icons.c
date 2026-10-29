#include "gui/icons.h"
#include "gui/theme.h"
#include "gui/gfx.h"
#include "gui/util.h"

static int g_x;
static int g_y;
static int g_s;

static int sc(int v)
{
    return v * g_s / 32;
}

static void box(int a, int b, int w, int h, uint32_t c)
{
    int x0 = g_x + sc(a);
    int y0 = g_y + sc(b);
    int x1 = g_x + sc(a + w);
    int y1 = g_y + sc(b + h);

    if(x1 <= x0)
        x1 = x0 + 1;

    if(y1 <= y0)
        y1 = y0 + 1;

    gfx_fill_rect(x0, y0, x1 - x0, y1 - y0, c);
}

static void line(int a, int b, int c2, int d, int thick, uint32_t color)
{
    int t = sc(thick);
    int i;

    if(t < 1)
        t = 1;

    for(i = 0; i < t; i++)
        gfx_line(g_x + sc(a) + i - t / 2, g_y + sc(b),
                 g_x + sc(c2) + i - t / 2, g_y + sc(d), color);

    for(i = 0; i < t; i++)
        gfx_line(g_x + sc(a), g_y + sc(b) + i - t / 2,
                 g_x + sc(c2), g_y + sc(d) + i - t / 2, color);
}

static void ring(int cx, int cy, int r, int t, int gap_top, uint32_t color)
{
    int pcx = g_x + sc(cx);
    int pcy = g_y + sc(cy);
    int pr = sc(r);
    int pt = sc(t);
    int gap = sc(3);
    int dx;
    int dy;

    if(pt < 1)
        pt = 1;

    for(dy = -pr; dy <= pr; dy++)
    {
        for(dx = -pr; dx <= pr; dx++)
        {
            int d2 = dx * dx + dy * dy;
            int inner = pr - pt;

            if(d2 > pr * pr || d2 < inner * inner)
                continue;

            if(gap_top && dy < 0 && dx >= -gap && dx <= gap)
                continue;

            gfx_fill_rect(pcx + dx, pcy + dy, 1, 1, color);
        }
    }
}

static void disc(int cx, int cy, int r, uint32_t color)
{
    gfx_fill_circle(g_x + sc(cx), g_y + sc(cy), sc(r) < 1 ? 1 : sc(r), color);
}

static void tri_right(int a, int b, int h, uint32_t color)
{
    int row;
    int ph = sc(h);

    for(row = 0; row < ph; row++)
    {
        int half = ph / 2;
        int d = row < half ? row : ph - 1 - row;

        gfx_fill_rect(g_x + sc(a), g_y + sc(b) + row, d * 2 / 2 + 1, 1, color);
    }
}

static void doc(uint32_t fill, uint32_t edge)
{
    box(6, 2, 20, 28, edge);
    box(7, 3, 18, 26, fill);
    box(19, 2, 7, 7, edge);
    box(20, 3, 5, 5, GFX_RGB(0xDD, 0xDD, 0xDD));
}

static void icon_folder(void)
{
    box(2, 7, 13, 6, GFX_RGB(0xD9, 0xA0, 0x1B));
    box(2, 10, 28, 18, GFX_RGB(0xFF, 0xCA, 0x28));
    box(2, 14, 28, 14, GFX_RGB(0xFF, 0xB3, 0x00));
}

static void icon_editor(void)
{
    box(4, 3, 24, 26, GFX_RGB(0x00, 0x78, 0xD7));
    box(5, 8, 22, 20, GFX_RGB(0xFF, 0xFF, 0xFF));
    box(8, 12, 16, 2, GFX_RGB(0x99, 0x99, 0x99));
    box(8, 16, 12, 2, GFX_RGB(0x99, 0x99, 0x99));
    box(8, 20, 15, 2, GFX_RGB(0x99, 0x99, 0x99));
    box(8, 24, 9, 2, GFX_RGB(0x99, 0x99, 0x99));
}

static void icon_terminal(void)
{
    box(2, 5, 28, 22, GFX_RGB(0x55, 0x55, 0x55));
    box(3, 8, 26, 18, GFX_RGB(0x0C, 0x0C, 0x0C));
    line(7, 12, 12, 16, 2, GFX_RGB(0x16, 0xC6, 0x0C));
    line(12, 16, 7, 20, 2, GFX_RGB(0x16, 0xC6, 0x0C));
    box(15, 20, 9, 2, GFX_RGB(0xF2, 0xF2, 0xF2));
}

static void icon_ide(void)
{
    uint32_t white = GFX_RGB(0xFF, 0xFF, 0xFF);

    box(2, 2, 28, 28, GFX_RGB(0x5C, 0x2D, 0x91));
    line(12, 10, 6, 16, 2, white);
    line(6, 16, 12, 22, 2, white);
    line(20, 10, 26, 16, 2, white);
    line(26, 16, 20, 22, 2, white);
    line(18, 8, 14, 24, 2, GFX_RGB(0xFF, 0xD1, 0x4D));
}

static void icon_pong(void)
{
    uint32_t white = GFX_RGB(0xFF, 0xFF, 0xFF);
    int i;

    box(2, 4, 28, 24, GFX_RGB(0x10, 0x10, 0x18));
    box(5, 10, 3, 11, white);
    box(24, 8, 3, 11, white);
    box(15, 13, 4, 4, GFX_RGB(0xFF, 0xD1, 0x4D));

    for(i = 0; i < 4; i++)
        box(15, 6 + i * 6, 2, 3, GFX_RGB(0x66, 0x66, 0x66));
}

static void icon_saver(void)
{
    box(3, 5, 26, 18, GFX_RGB(0x2B, 0x2B, 0x2B));
    box(5, 7, 22, 14, GFX_RGB(0x00, 0x78, 0xD7));
    box(8, 10, 3, 3, GFX_RGB(0xFF, 0xFF, 0xFF));
    box(18, 14, 3, 3, GFX_RGB(0xFF, 0xFF, 0xFF));
    box(22, 9, 2, 2, GFX_RGB(0xFF, 0xD1, 0x4D));
    box(14, 23, 4, 3, GFX_RGB(0x2B, 0x2B, 0x2B));
    box(9, 26, 14, 2, GFX_RGB(0x2B, 0x2B, 0x2B));
}

static void icon_tex(void)
{
    doc(GFX_RGB(0xFF, 0xFF, 0xFF), GFX_RGB(0x80, 0x80, 0x80));
    tri_right(11, 11, 14, GFX_RGB(0x10, 0x7C, 0x10));
    box(10, 25, 12, 2, GFX_RGB(0xAA, 0xAA, 0xAA));
}

static void icon_tix(void)
{
    uint32_t c = GFX_RGB(0x5C, 0x2D, 0x91);

    doc(GFX_RGB(0xFF, 0xFF, 0xFF), GFX_RGB(0x80, 0x80, 0x80));
    line(14, 12, 10, 17, 2, c);
    line(10, 17, 14, 22, 2, c);
    line(18, 12, 22, 17, 2, c);
    line(22, 17, 18, 22, 2, c);
}

static void icon_user(void)
{
    uint32_t c = GFX_RGB(0xFF, 0xFF, 0xFF);
    int cx0;
    int cy0;
    int cx1;
    int cy1;
    int sx;
    int sy;
    int sw;
    int sh;

    gfx_clip_get(&cx0, &cy0, &cx1, &cy1);
    sx = maxi(cx0, g_x);
    sy = maxi(cy0, g_y);
    sw = mini(cx1, g_x + g_s) - sx;
    sh = mini(cy1, g_y + g_s) - sy;

    if(sw <= 0 || sh <= 0)
        return;

    gfx_clip_set(sx, sy, sw, sh);
    disc(16, 11, 6, c);
    disc(16, 31, 12, c);
    gfx_clip_set(cx0, cy0, cx1 - cx0, cy1 - cy0);
}

static void icon_power(uint32_t c)
{
    ring(16, 17, 10, 2, 1, c);
    box(15, 4, 2, 13, c);
}

static void icon_signout(uint32_t c)
{
    box(5, 5, 2, 22, c);
    box(5, 5, 10, 2, c);
    box(5, 25, 10, 2, c);
    box(12, 15, 15, 2, c);
    line(22, 10, 27, 16, 2, c);
    line(27, 16, 22, 22, 2, c);
}

static void icon_restart(uint32_t c)
{
    ring(16, 17, 10, 2, 1, c);
    box(14, 3, 2, 2, c);
    line(16, 4, 21, 8, 2, c);
    line(16, 4, 21, 0, 2, c);
}

static void icon_plus(uint32_t c)
{
    box(15, 6, 2, 20, c);
    box(6, 15, 20, 2, c);
}

static void icon_back(uint32_t c)
{
    line(22, 6, 10, 16, 3, c);
    line(10, 16, 22, 26, 3, c);
}

static void icon_up(uint32_t c)
{
    line(6, 22, 16, 10, 3, c);
    line(16, 10, 26, 22, 3, c);
}

static void icon_delete(uint32_t c)
{
    box(7, 8, 18, 2, c);
    box(13, 5, 6, 3, c);
    box(9, 10, 2, 17, c);
    box(21, 10, 2, 17, c);
    box(9, 25, 14, 2, c);
    box(15, 13, 2, 11, c);
}

static void icon_save(uint32_t c)
{
    box(6, 6, 20, 2, c);
    box(6, 24, 20, 2, c);
    box(6, 6, 2, 20, c);
    box(24, 6, 2, 20, c);
    box(11, 6, 10, 7, c);
    box(10, 17, 12, 9, c);
}

static void icon_build(uint32_t c)
{
    line(7, 25, 19, 13, 3, c);
    box(17, 5, 9, 9, c);
}

static void icon_run(uint32_t c)
{
    tri_right(9, 5, 22, c);
}

static void icon_sun(uint32_t c)
{
    disc(16, 16, 6, c);
    line(16, 2, 16, 6, 2, c);
    line(16, 26, 16, 30, 2, c);
    line(2, 16, 6, 16, 2, c);
    line(26, 16, 30, 16, 2, c);
    line(6, 6, 9, 9, 2, c);
    line(23, 23, 26, 26, 2, c);
    line(26, 6, 23, 9, 2, c);
    line(9, 23, 6, 26, 2, c);
}

static void icon_moon(uint32_t c)
{
    int dx;
    int dy;
    int r = sc(10);
    int r2 = sc(9);
    int ax = g_x + sc(15);
    int ay = g_y + sc(17);
    int bx = g_x + sc(21);
    int by = g_y + sc(13);

    for(dy = -r; dy <= r; dy++)
    {
        for(dx = -r; dx <= r; dx++)
        {
            int px = ax + dx;
            int py = ay + dy;
            int ex = px - bx;
            int ey = py - by;

            if(dx * dx + dy * dy > r * r)
                continue;

            if(ex * ex + ey * ey <= r2 * r2)
                continue;

            gfx_fill_rect(px, py, 1, 1, c);
        }
    }
}

static void icon_newfile(uint32_t c)
{
    box(7, 3, 2, 26, c);
    box(7, 3, 12, 2, c);
    box(7, 27, 18, 2, c);
    box(23, 11, 2, 18, c);
    box(19, 3, 2, 8, c);
    box(19, 9, 6, 2, c);
    line(20, 3, 25, 9, 1, c);
}

static void icon_newfolder(uint32_t c)
{
    box(3, 8, 11, 2, c);
    box(3, 8, 2, 19, c);
    box(3, 25, 26, 2, c);
    box(27, 12, 2, 15, c);
    box(14, 12, 15, 2, c);
    box(14, 8, 2, 4, c);
    box(14, 17, 2, 7, c);
    box(11, 20, 8, 2, c);
}

static void icon_rename(uint32_t c)
{
    line(8, 24, 22, 10, 3, c);
    box(22, 6, 4, 4, c);
    box(6, 26, 6, 2, c);
}

static void icon_start(uint32_t c)
{
    int yy;
    int y0 = g_y + sc(4);
    int y1 = g_y + sc(28);
    int w = sc(4) < 1 ? 1 : sc(4);

    if(c == 0)
        c = C_ACCENT;

    box(8, 4, 5, 23, c);
    box(3, 10, 13, 4, c);
    box(8, 23, 9, 4, c);
    box(13, 20, 4, 7, c);

    for(yy = y0; yy < y1; yy++)
        gfx_fill_rect(g_x + sc(19) + (yy - y0) * sc(9) / (y1 - y0), yy, w, 1, c);
}

void icon_draw(int id, int x, int y, int size, uint32_t color)
{
    g_x = x;
    g_y = y;
    g_s = size;

    switch(id)
    {
        case ICON_FOLDER: icon_folder(); break;
        case ICON_FILE: doc(GFX_RGB(0xFF, 0xFF, 0xFF), GFX_RGB(0x80, 0x80, 0x80)); box(10, 12, 12, 2, GFX_RGB(0xAA, 0xAA, 0xAA)); box(10, 17, 12, 2, GFX_RGB(0xAA, 0xAA, 0xAA)); box(10, 22, 8, 2, GFX_RGB(0xAA, 0xAA, 0xAA)); break;
        case ICON_EDITOR: icon_editor(); break;
        case ICON_TERMINAL: icon_terminal(); break;
        case ICON_IDE: icon_ide(); break;
        case ICON_PONG: icon_pong(); break;
        case ICON_SAVER: icon_saver(); break;
        case ICON_TEX: icon_tex(); break;
        case ICON_TIX: icon_tix(); break;
        case ICON_USER: icon_user(); break;
        case ICON_POWER: icon_power(color); break;
        case ICON_SIGNOUT: icon_signout(color); break;
        case ICON_RESTART: icon_restart(color); break;
        case ICON_PLUS: icon_plus(color); break;
        case ICON_BACK: icon_back(color); break;
        case ICON_UP: icon_up(color); break;
        case ICON_DELETE: icon_delete(color); break;
        case ICON_SAVE: icon_save(color); break;
        case ICON_BUILD: icon_build(color); break;
        case ICON_RUN: icon_run(color); break;
        case ICON_SUN: icon_sun(color); break;
        case ICON_MOON: icon_moon(color); break;
        case ICON_NEWFILE: icon_newfile(color); break;
        case ICON_NEWFOLDER: icon_newfolder(color); break;
        case ICON_RENAME: icon_rename(color); break;
        case ICON_START: icon_start(color); break;
    }
}
