#ifndef TURBOS_TERMINAL_H
#define TURBOS_TERMINAL_H

#include "apps/app.h"

void terminal_print(const char *s);
void draw_terminal(void);
void handle_terminal_key(int key);

extern const app_t terminal_app;

#endif
