#include "ui/saver.h"
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
