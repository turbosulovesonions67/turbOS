#ifndef PS2_H
#define PS2_H

void ps2_init(void);
int ps2_wait_input(void);
int ps2_wait_output(void);
void ps2_write(unsigned char value);
unsigned char ps2_read(void);
void ps2_write_mouse(unsigned char value);

#endif
