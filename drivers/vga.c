#include "vga.h"

static volatile unsigned char *vga = (volatile unsigned char *)0xB8000;

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

void vga_clear(unsigned char color)
{
    for(int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = color;
    }
}

void vga_put(char c, int row, int col, unsigned char color)
{
    if(row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH)
        return;

    int i = (row * VGA_WIDTH + col) * 2;

    vga[i] = c;
    vga[i + 1] = color;
}

void vga_print(const char *s, int row, int col, unsigned char color)
{
    for(int i = 0; s[i] && col + i < VGA_WIDTH; i++)
        vga_put(s[i], row, col + i, color);
}

void vga_cursor(int row, int col)
{
    if(row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH)
        return;

    unsigned short position = row * VGA_WIDTH + col;

    outb(0x3D4, 0x0F);
    outb(0x3D5, position & 0xFF);

    outb(0x3D4, 0x0E);
    outb(0x3D5, (position >> 8) & 0xFF);

    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0E);

    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
}

void vga_cursor_hide(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}
