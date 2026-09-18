#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void vga_clear(unsigned char color);
void vga_put(char c, int row, int col, unsigned char color);
void vga_print(const char *s, int row, int col, unsigned char color);
void vga_cursor(int row, int col);
void vga_cursor_hide(void);

#endif
