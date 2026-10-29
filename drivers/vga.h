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

#define VGA_SURF_APPS    6
#define VGA_SURF_CONSOLE 6
#define VGA_SURF_COUNT   7

void vga_console_begin(void);
void vga_console_end(void);

void vga_desktop_enter(void);

void vga_surface_override(int surface);

unsigned int vga_console_epoch(void);

unsigned short vga_surface_cell(int surface, int row, int col);
int vga_surface_cursor(int surface, int *row, int *col);
unsigned int vga_surface_version(int surface);

uint32_t vga_color(int index);

#endif
