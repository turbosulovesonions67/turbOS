#include "time/rtc.h"

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

unsigned char rtc_bcd_to_bin(unsigned char value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

unsigned char rtc_read(unsigned char reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

void rtc_get_time(
    unsigned char *hour,
    unsigned char *minute,
    unsigned char *second)
{
    *hour = rtc_bcd_to_bin(rtc_read(0x04));
    *minute = rtc_bcd_to_bin(rtc_read(0x02));
    *second = rtc_bcd_to_bin(rtc_read(0x00));
}
