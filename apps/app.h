#ifndef TURBOS_APP_H
#define TURBOS_APP_H

#include <stdint.h>

#define APP_COUNT 5

#define APP_TICK_VISIBLE 0   
#define APP_TICK_FOCUS   1 

#define APP_WHITE ((uint32_t)0xFFFFFFu)

typedef struct
{
    int screen;                 
    const char *title;          
    const char *label;          
    void (*draw)(int x, int y, int w, int h);
    void (*mouse)(int x, int y, int w, int h, int mx, int my);
    void (*key)(int key);       
    void (*icon)(int x, int y, int s);   
    void (*tick)(void);         
    int tick_period;            
    int tick_when;              
    int redraw_all;             
    void (*text_redraw)(void);  
} app_t;

extern const app_t *const app_table[APP_COUNT];

const app_t *app_by_screen(int screen);

int app_icon_sc(int v, int s);
void app_icon_box(int x, int y, int s,
                  int ax, int ay, int aw, int ah, uint32_t c);

#endif
