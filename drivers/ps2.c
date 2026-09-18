#include "ps2.h"

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

static unsigned char inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile("inb %1,%0" : "=a"(value) : "Nd"(port));

    return value;
}

int ps2_wait_input(void)
{
    unsigned int timeout = 100000;

    while ((inb(0x64) & 2) && timeout--)
    {
    }

    return timeout != 0;
}

int ps2_wait_output(void)
{
    unsigned int timeout = 100000;

    while (!(inb(0x64) & 1) && timeout--)
    {
    }

    return timeout != 0;
}

void ps2_write(unsigned char value)
{
    if (ps2_wait_input())
        outb(0x60, value);
}

unsigned char ps2_read(void)
{
    if (!ps2_wait_output())
        return 0;

    return inb(0x60);
}

void ps2_write_mouse(unsigned char value)
{
    if (!ps2_wait_input())
        return;

    outb(0x64, 0xD4);

    if (!ps2_wait_input())
        return;

    outb(0x60, value);
}

void ps2_init(void)
{
    unsigned char status;
    unsigned int timeout;

    if (!ps2_wait_input())
        return;

    outb(0x64, 0xAD);

    if (!ps2_wait_input())
        return;

    outb(0x64, 0xA7);

    timeout = 100000;

    while ((inb(0x64) & 1) && timeout--)
        inb(0x60);

    if (!ps2_wait_input())
        return;

    outb(0x64, 0x20);

    if (!ps2_wait_output())
        return;

    status = inb(0x60);

    status |= 0x01;
    status |= 0x02;
    status &= ~0x20;

    if (!ps2_wait_input())
        return;

    outb(0x64, 0x60);

    if (!ps2_wait_input())
        return;

    outb(0x60, status);

    if (!ps2_wait_input())
        return;

    outb(0x64, 0xAE);

    if (!ps2_wait_input())
        return;

    outb(0x64, 0xA8);
}
