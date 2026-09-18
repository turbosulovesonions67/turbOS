#include "idt.h"
#include "isr.h"
#include "../drivers/pic.h"
#include "../drivers/pit.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

static void idt_set_gate(unsigned char num, unsigned long base)
{
    idt[num].base_low = base & 0xFFFF;
    idt[num].selector = 0x08;
    idt[num].zero = 0;
    idt[num].flags = 0x8E;
    idt[num].base_high = (base >> 16) & 0xFFFF;
}

static void idt_load(void)
{
    __asm__ volatile ("lidt (%0)" : : "r" (&idtp));
}

void isr0_handler(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void irq0_handler(void)
{
    pit_tick();
    pic_send_eoi(0);
}

void irq1_handler(void)
{
    keyboard_irq();
    pic_send_eoi(1);
}

void irq12_handler(void)
{
    mouse_irq();
    pic_send_eoi(12);
}

void idt_init(void)
{
    unsigned int i;

    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base = (unsigned long)&idt;

    for (i = 0; i < 256; i++) {
        idt[i].base_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].flags = 0;
        idt[i].base_high = 0;
    }

    idt_set_gate(0, (unsigned long)isr0);
    idt_set_gate(32, (unsigned long)irq0);
    idt_set_gate(33, (unsigned long)irq1);
    idt_set_gate(44, (unsigned long)irq12);

    idt_load();
}
