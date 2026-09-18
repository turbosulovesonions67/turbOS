#include "pit.h"

volatile unsigned int timer_ticks = 0;

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void pit_init(unsigned int frequency) {
    if (frequency == 0) {
        frequency = 100;
    }

    unsigned int divisor = 1193180 / frequency;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    timer_ticks = 0;
}

void pit_tick(void) {
    timer_ticks++;
}
