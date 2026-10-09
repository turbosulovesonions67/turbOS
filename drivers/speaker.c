#include "speaker.h"

#define PIT_FREQUENCY 1193182
#define PIT_CH2       0x42
#define PIT_COMMAND   0x43
#define SPEAKER_PORT  0x61

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

static void speaker_delay(unsigned int milliseconds)
{
    volatile unsigned int count;

    while(milliseconds--)
    {
        for(count = 0; count < 10000; count++)
            __asm__ volatile("nop");
    }
}

void speaker_start(unsigned int frequency)
{
    unsigned int divisor;

    if(frequency == 0)
    {
        speaker_stop();
        return;
    }

    divisor = PIT_FREQUENCY / frequency;

    if(divisor == 0)
        divisor = 1;

    if(divisor > 0xFFFF)
        divisor = 0xFFFF;

    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CH2, divisor & 0xFF);
    outb(PIT_CH2, (divisor >> 8) & 0xFF);
    outb(SPEAKER_PORT, inb(SPEAKER_PORT) | 0x03);
}

void speaker_stop(void)
{
    outb(SPEAKER_PORT, inb(SPEAKER_PORT) & 0xFC);
}

void speaker_play(unsigned int frequency, unsigned int milliseconds)
{
    speaker_start(frequency);
    speaker_delay(milliseconds);
    speaker_stop();
}
