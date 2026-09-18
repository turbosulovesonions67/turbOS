#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_NONE 0
#define KEY_ESC 27
#define KEY_BACKSPACE 8
#define KEY_TAB 9
#define KEY_ENTER 10
#define KEY_UP 256
#define KEY_DOWN 257
#define KEY_LEFT 258
#define KEY_RIGHT 259
#define KEY_CTRL 260
#define KEY_SHIFT 261
#define KEY_F1 262
#define KEY_F2 263
#define KEY_F3 264
#define KEY_F4 265
#define KEY_F5 266
#define KEY_F6 267
#define KEY_F7 268
#define KEY_F8 269
#define KEY_F9 270
#define KEY_F10 271

void keyboard_init(void);
void keyboard_irq(void);
int keyboard_getkey(void);

#endif
