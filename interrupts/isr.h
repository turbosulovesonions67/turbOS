#ifndef ISR_H
#define ISR_H

extern void isr0(void);
extern void irq0(void);
extern void irq1(void);
extern void irq12(void);

void isr0_handler(void);
void irq0_handler(void);
void irq1_handler(void);
void irq12_handler(void);

#endif
