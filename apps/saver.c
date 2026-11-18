#include "apps/saver.h"
#include "apps/app.h"
#include "gui/gfx.h"
#include "kernel/state.h"
#include "drivers/vga.h"

void draw_saver(void)
{
    vga_clear(0x01);

    vga_print(
        "Ctrl+2 Editor Ctrl+3 Pong Ctrl+4 Terminal",
        0,
        0,
        0x0F);

    vga_print("turbOS", saver_y, saver_x, 0x0F);

    vga_cursor_hide();
}

void update_saver(void)
{
    saver_x += saver_dx;
    saver_y += saver_dy;

    if(saver_x <= 0 || saver_x >= 74)
        saver_dx = -saver_dx;

    if(saver_y <= 1 || saver_y >= 24)
        saver_dy = -saver_dy;
}

static void sv_tick(void)
{
    vga_surface_override(SCREEN_SAVER);
    update_saver();
    draw_saver();
    vga_surface_override(-1);
}

#define SV_BG GFX_RGB(12, 16, 40)
#define SV_STAR GFX_RGB(110, 120, 170)
#define SV_PILL GFX_RGB(0, 120, 215)
#define SV_PILL_EDGE GFX_RGB(120, 190, 250)
#define SV_DIM GFX_RGB(140, 150, 190)
#define SV_PILL_H 36
#define SV_STARS 14

static void saver_draw(int x, int y, int w, int h)
{
    int pill_w = gfx_text_width("turbOS") + 32;
    int px = x + saver_x * (w - pill_w) / 74;
    int py = y + (saver_y - 1) * (h - SV_PILL_H) / 23;
    int i;

    gfx_fill_rect(x, y, w, h, SV_BG);

    for(i = 0; i < SV_STARS; i++)
        gfx_fill_rect(x + (i * 37 + 11) % 97 * w / 97,
                      y + (i * 53 + 7) % 89 * h / 89, 2, 2, SV_STAR);

    gfx_fill_rect(px, py, pill_w, SV_PILL_H, SV_PILL);
    gfx_rect_outline(px, py, pill_w, SV_PILL_H, SV_PILL_EDGE);
    gfx_draw_text(px + 16, py + 10, "turbOS", APP_WHITE);

    gfx_draw_text(x + (w - gfx_text_width("Ctrl+2 Editor  Ctrl+3 Pong  Ctrl+4 Terminal")) / 2,
                  y + h - 28, "Ctrl+2 Editor  Ctrl+3 Pong  Ctrl+4 Terminal", SV_DIM);
}

static void saver_icon(int x, int y, int s)
{
        app_icon_box(x, y, s, 2, 4, 28, 24, GFX_RGB(31, 45, 99));
        gfx_fill_circle(x + app_icon_sc(15, s), y + app_icon_sc(16, s), app_icon_sc(7, s),
                        GFX_RGB(255, 216, 107));
        gfx_fill_circle(x + app_icon_sc(19, s), y + app_icon_sc(14, s), app_icon_sc(6, s),
                        GFX_RGB(31, 45, 99));
        app_icon_box(x, y, s, 24, 8, 2, 2, APP_WHITE);
        app_icon_box(x, y, s, 8, 9, 2, 2, APP_WHITE);
        app_icon_box(x, y, s, 24, 22, 2, 2, APP_WHITE);
}

const app_t saver_app =
{
    SCREEN_SAVER, "Screen Saver", "Saver",
    saver_draw, 0,
    0, saver_icon,
    sv_tick, 20, APP_TICK_VISIBLE,
    0, draw_saver
};
