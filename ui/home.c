#include "ui/home.h"
#include "kernel/state.h"
#include "time/rtc.h"
#include "drivers/vga.h"


static void print_local(
    const char *s,
    int row,
    int col,
    unsigned char color)
{
    vga_print(s, row, col, color);
}

static void draw_clock(void)
{
    unsigned char h;
    unsigned char m;
    unsigned char s;
    char time[9];

    rtc_get_time(&h, &m, &s);

    time[0] = '0' + h / 10;
    time[1] = '0' + h % 10;
    time[2] = ':';
    time[3] = '0' + m / 10;
    time[4] = '0' + m % 10;
    time[5] = ':';
    time[6] = '0' + s / 10;
    time[7] = '0' + s % 10;
    time[8] = 0;

    print_local("Time:", 4, 0, 0x0F);
    print_local(time, 4, 6, 0x0F);
}

void draw_home(void)
{
    unsigned char h;

    vga_clear(home_color);

    print_local("Welcome to turbOS!", 0, 0, 0x0F);

    h = rtc_bcd_to_bin(rtc_read(0x04));

    if(h < 12)
        print_local("Good Morning!", 2, 0, 0x0F);
    else if(h < 17)
        print_local("Good Afternoon!", 2, 0, 0x0F);
    else if(h < 21)
        print_local("Good Evening!", 2, 0, 0x0F);
    else
        print_local("Good Night!", 2, 0, 0x0F);

    draw_clock();

    print_local("Ctrl+1 Home", 7, 0, 0x0F);
    print_local("Ctrl+2 Saver", 8, 0, 0x0F);
    print_local("Ctrl+3 Editor", 9, 0, 0x0F);
    print_local("Ctrl+4 Pong", 10, 0, 0x0F);
    print_local("Ctrl+5 Terminal", 11, 0, 0x0F);
    print_local("Ctrl+6 File Manager", 12, 0, 0x0F);
    print_local("Ctrl+9 Reboot", 13, 0, 0x0F);
    print_local("Ctrl+0 Shutdown", 14, 0, 0x0F);

    vga_cursor_hide();
}

void home_update_clock(void)
{
    draw_clock();
}
