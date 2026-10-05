#include "power/power.h"

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

static void outw(unsigned short port, unsigned short value)
{
    __asm__ volatile("outw %0,%1" : : "a"(value), "Nd"(port));
}

void power_halt(void)
{
    for(;;)
        __asm__ volatile("hlt");
}

void power_reboot(void)
{
    outb(0x64, 0xFE);
    power_halt();
}

void power_shutdown(void)
{
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    power_halt();
}
