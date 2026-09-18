#ifndef IDT_H
#define IDT_H

struct idt_entry {
    unsigned short base_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

extern struct idt_entry idt[256];
extern struct idt_ptr idtp;

void idt_init(void);

void isr0_handler(void);
void irq0_handler(void);
void irq1_handler(void);
void irq12_handler(void);

#endif
