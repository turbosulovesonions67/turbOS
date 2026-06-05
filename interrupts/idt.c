#include "idt.h"
#include "isr.h"

extern void put_char(char c, int r, int col, unsigned char color);

static struct idt_entry idt[256];
static struct idt_ptr idtp;

static void idt_set_gate(
    unsigned char num,
    unsigned int base,
    unsigned short sel,
    unsigned char flags)
{
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

static void idt_load()
{
    __asm__ volatile("lidt %0" : : "m"(idtp));
}

void isr0_handler(void)
{
    put_char('I', 20, 0, 0x0A);
    put_char('S', 20, 1, 0x0A);
    put_char('R', 20, 2, 0x0A);
    put_char('0', 20, 3, 0x0A);
}

void irq0_handler(void)
{
    put_char('T', 21, 0, 0x0A);
}

void idt_init(void)
{
    for(int i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    idt_set_gate(
        0,
        (unsigned int)isr0,
        0x08,
        0x8E
    );

    idt_set_gate(
        32,
        (unsigned int)irq0,
        0x08,
        0x8E
    );

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned int)&idt;

    idt_load();
}
