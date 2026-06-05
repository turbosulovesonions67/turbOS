#include "pic.h"

void outb(unsigned short port, unsigned char val);

#define PIC1 0x20
#define PIC2 0xA0

#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)

#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)

void pic_remap(void)
{
    unsigned char a1 = 0xFF;
    unsigned char a2 = 0xFF;

    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_send_eoi(unsigned char irq)
{
    if(irq >= 8)
        outb(PIC2_COMMAND, 0x20);

    outb(PIC1_COMMAND, 0x20);
}
