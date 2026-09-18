#include "pic.h"

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

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

void pic_remap(void)
{
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, 0xF8);
    outb(PIC2_DATA, 0xEF);
}

void pic_set_mask(unsigned char irq)
{
    unsigned short port;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        irq -= 8;
        port = PIC2_DATA;
    }

    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(unsigned char irq)
{
    unsigned short port;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        irq -= 8;
        port = PIC2_DATA;
    }

    outb(port, inb(port) & ~(1 << irq));
}

void pic_send_eoi(unsigned char irq)
{
    if (irq >= 8)
        outb(PIC2_COMMAND, 0x20);

    outb(PIC1_COMMAND, 0x20);
}
