#ifndef MOUSE_H
#define MOUSE_H

typedef struct
{
    int x;
    int y;
    int left;
    int right;
    int middle;
} mouse_state_t;

void mouse_init(void);
void mouse_irq(void);
mouse_state_t mouse_get_state(void);

void mouse_set_bounds(int max_x, int max_y);
void mouse_set_position(int x, int y);

#endif
