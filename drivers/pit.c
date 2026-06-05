#include "pit.h"

void outb(unsigned short port, unsigned char val);

volatile unsigned int timer_ticks = 0;

void pit_init(unsigned int frequency)
{
    unsigned int divisor = 1193180 / frequency;

    outb(0x43, 0x36);

    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}
