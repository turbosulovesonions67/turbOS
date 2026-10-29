#include "vga.h"
#include "gui/gfx.h"
#include "kernel/state.h"

static volatile unsigned char *vga = (volatile unsigned char *)0xB8000;

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

typedef struct
{
    unsigned short cells[VGA_WIDTH * VGA_HEIGHT];
    int cursor_row;
    int cursor_col;
    int cursor_visible;
    unsigned int version;
} vga_surface_t;

static vga_surface_t surfaces[VGA_SURF_COUNT];

static const uint32_t palette[16] = {
    0x0C0C0C, 0x0037DA, 0x13A10E, 0x3A96DD,
    0xC50F1F, 0x881798, 0xC19C00, 0xCCCCCC,
    0x767676, 0x3B78FF, 0x16C60C, 0x61D6D6,
    0xE74856, 0xB4009E, 0xF9F1A5, 0xF2F2F2
};

#define CONSOLE_NEST_MAX 4

static int console_mode = 1;
static int override_surface = -1;
static int console_depth;
static int console_saved[CONSOLE_NEST_MAX];
static unsigned int console_epoch_counter;

uint32_t vga_color(int index)
{
    return palette[index & 0x0F];
}

static int target_surface(void)
{
    if(console_mode)
        return VGA_SURF_CONSOLE;

    if(override_surface >= 0 && override_surface < VGA_SURF_APPS)
        return override_surface;

    if(current_screen >= 0 && current_screen < VGA_SURF_APPS)
        return current_screen;

    return 0;
}

static void console_draw_cell(int row, int col, unsigned short cell)
{
    int x0 = (gfx_width() - VGA_WIDTH * 8) / 2;
    int y0 = (gfx_height() - VGA_HEIGHT * 16) / 2;

    if(x0 < 0)
        x0 = 0;
    if(y0 < 0)
        y0 = 0;

    gfx_fb_draw_cell(x0 + col * 8,
                     y0 + row * 16,
                     (char)(cell & 0xFF),
                     palette[(cell >> 8) & 0x0F],
                     palette[(cell >> 12) & 0x0F]);
}

static void console_reset(void)
{
    int i;

    gfx_fb_clear(0x000000);

    for(i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        surfaces[VGA_SURF_CONSOLE].cells[i] = 0xFFFF;
}

static void surface_put(char c, int row, int col, unsigned char color)
{
    int s = target_surface();
    int i = row * VGA_WIDTH + col;
    unsigned short cell =
        (unsigned short)(((unsigned int)color << 8) | (unsigned char)c);

    if(surfaces[s].cells[i] == cell)
        return;

    surfaces[s].cells[i] = cell;
    surfaces[s].version++;

    if(s == VGA_SURF_CONSOLE)
        console_draw_cell(row, col, cell);
}

void vga_clear(unsigned char color)
{
    if(gfx_ready())
    {
        int row;
        int col;

        for(row = 0; row < VGA_HEIGHT; row++)
            for(col = 0; col < VGA_WIDTH; col++)
                surface_put(' ', row, col, color);

        return;
    }

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

    if(gfx_ready())
    {
        surface_put(c, row, col, color);
        return;
    }

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

    if(gfx_ready())
    {
        vga_surface_t *surf = &surfaces[target_surface()];

        if(!surf->cursor_visible ||
           surf->cursor_row != row || surf->cursor_col != col)
        {
            surf->cursor_visible = 1;
            surf->cursor_row = row;
            surf->cursor_col = col;
            surf->version++;
        }

        return;
    }

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
    if(gfx_ready())
    {
        vga_surface_t *surf = &surfaces[target_surface()];

        if(surf->cursor_visible)
        {
            surf->cursor_visible = 0;
            surf->version++;
        }

        return;
    }

    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_console_begin(void)
{
    if(!gfx_ready())
        return;

    if(console_depth < CONSOLE_NEST_MAX)
        console_saved[console_depth] = console_mode;

    console_depth++;

    if(!console_mode)
    {
        console_mode = 1;
        console_reset();
    }
}

void vga_console_end(void)
{
    int previous;

    if(!gfx_ready() || console_depth <= 0)
        return;

    console_depth--;

    previous = console_depth < CONSOLE_NEST_MAX ?
               console_saved[console_depth] : 1;

    if(!previous && console_mode)
    {
        console_mode = 0;
        console_epoch_counter++;
    }
}

void vga_desktop_enter(void)
{
    console_depth = 0;
    console_mode = 0;
    override_surface = -1;
    console_epoch_counter++;
}

void vga_surface_override(int surface)
{
    override_surface = surface;
}

unsigned int vga_console_epoch(void)
{
    return console_epoch_counter;
}

unsigned short vga_surface_cell(int surface, int row, int col)
{
    if(surface < 0 || surface >= VGA_SURF_COUNT ||
       row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH)
        return 0x0720;

    return surfaces[surface].cells[row * VGA_WIDTH + col];
}

int vga_surface_cursor(int surface, int *row, int *col)
{
    if(surface < 0 || surface >= VGA_SURF_COUNT ||
       !surfaces[surface].cursor_visible)
        return 0;

    *row = surfaces[surface].cursor_row;
    *col = surfaces[surface].cursor_col;

    return 1;
}

unsigned int vga_surface_version(int surface)
{
    if(surface < 0 || surface >= VGA_SURF_COUNT)
        return 0;

    return surfaces[surface].version;
}
