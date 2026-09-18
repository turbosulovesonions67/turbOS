#ifndef PIT_H
#define PIT_H

extern volatile unsigned int timer_ticks;

void pit_init(unsigned int frequency);
void pit_tick(void);

#endif
