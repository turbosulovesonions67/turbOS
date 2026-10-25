#include "framebuffer.h"

#define MB2_MAGIC 0x36D76289U

static const unsigned char font[26][7] = {
    {14,17,17,31,17,17,17},
    {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},
    {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},
    {17,17,17,31,17,17,17},
    {31,4,4,4,4,4,31},
    {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},
    {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},
    {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},
    {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},
    {31,1,2,4,8,16,31}
};

static void pixel(volatile unsigned char *fb, unsigned int pitch,
                  unsigned int x, unsigned int y, unsigned int color)
{
    *(volatile unsigned int *)(fb + y * pitch + x * 4) = color;
}

static void fill(volatile unsigned char *fb, unsigned int pitch,
                 unsigned int width, unsigned int height,
                 unsigned int color)
{
    unsigned int x, y;

    for(y = 0; y < height; y++)
        for(x = 0; x < width; x++)
            pixel(fb, pitch, x, y, color);
}

static void draw_text(volatile unsigned char *fb, unsigned int pitch,
                      unsigned int width, unsigned int height,
                      const char *s, unsigned int scale,
                      unsigned int color, unsigned int y)
{
    static const unsigned char capital_i[7] =
        {31,4,4,4,4,4,31};

    unsigned int length = 0, i, cursor;

    while(s[length])
        length++;

    if(length * 6 * scale > width || y + 7 * scale > height)
        return;

    cursor = (width - length * 6 * scale) / 2;

    for(i = 0; i < length; i++)
    {
        unsigned int row, col, sx, sy;
        char c = s[i];

        if(c == ' ')
        {
            cursor += 6 * scale;
            continue;
        }

        for(row = 0; row < 7; row++)
        {
            unsigned char bits = 0;

            if(c >= 'a' && c <= 'z')
                bits = font[c - 'a'][row];
            else if(c == 'I')
                bits = capital_i[row];

            for(col = 0; col < 5; col++)
            {
                if(!(bits & (1U << (4 - col))))
                    continue;

                for(sy = 0; sy < scale; sy++)
                    for(sx = 0; sx < scale; sx++)
                        pixel(fb, pitch,
                              cursor + col * scale + sx,
                              y + row * scale + sy,
                              color);
            }
        }

        cursor += 6 * scale;
    }
}

void framebuffer_boot_message(unsigned int magic, unsigned int mbi_addr)
{
    unsigned int total, offset;

    if(magic != MB2_MAGIC || !mbi_addr)
        return;

    total = *(unsigned int *)mbi_addr;

    if(total < 16)
        return;

    for(offset = 8; offset + 8 <= total;)
    {
        unsigned int *tag = (unsigned int *)(mbi_addr + offset);
        unsigned int type = tag[0], size = tag[1];

        if(size < 8 || size > total - offset)
            return;

        if(type == 0)
            return;

        if(type == 8 && size >= 32)
        {
            unsigned char *info = (unsigned char *)tag;
            unsigned int addr_low = tag[2];
            unsigned int addr_high = tag[3];
            unsigned int pitch = tag[4];
            unsigned int width = tag[5];
            unsigned int height = tag[6];
            unsigned int bpp = info[28];
            unsigned int fb_type = info[29];
            volatile unsigned char *fb;
            unsigned int i;

            if(addr_high || !addr_low || bpp != 32 ||
               fb_type != 1 || !width || !height ||
               width > 0x3fffffffU || pitch < width * 4)
                return;

            fb = (volatile unsigned char *)addr_low;

            fill(fb, pitch, width, height, 0x000080D0);

            for(i = 0; i < 4 && i < width / 2 && i < height / 2; i++)
            {
                unsigned int x, y;

                for(x = i; x < width - i; x++)
                {
                    pixel(fb, pitch, x, i, 0x0000FFFF);
                    pixel(fb, pitch, x, height - 1 - i, 0x0000FFFF);
                }

                for(y = i; y < height - i; y++)
                {
                    pixel(fb, pitch, i, y, 0x0000FFFF);
                    pixel(fb, pitch, width - 1 - i, y, 0x0000FFFF);
                }
            }

            draw_text(fb, pitch, width, height,
                      "framebuffer test", 5, 0x00FFFFFF,
                      height / 2 - 60);

            draw_text(fb, pitch, width, height,
                      "I implemented framebuffer yeah",
                      2, 0x00FFFF00, height / 2 + 15);

            return;
        }

        offset = (offset + size + 7U) & ~7U;
    }
}
