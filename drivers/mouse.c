#include "mouse.h"
#include "ps2.h"

static volatile mouse_state_t mouse;
static unsigned char packet[3];
static int packet_index = 0;

static volatile int bound_x = 79;
static volatile int bound_y = 24;

void mouse_set_bounds(int max_x, int max_y)
{
    unsigned int flags;

    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");

    bound_x = max_x;
    bound_y = max_y;

    if (mouse.x > bound_x)
        mouse.x = bound_x;

    if (mouse.y > bound_y)
        mouse.y = bound_y;

    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

void mouse_set_position(int x, int y)
{
    unsigned int flags;

    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");

    mouse.x = x < 0 ? 0 : (x > bound_x ? bound_x : x);
    mouse.y = y < 0 ? 0 : (y > bound_y ? bound_y : y);

    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

void mouse_init(void)
{
    mouse.x = 40;
    mouse.y = 12;
    mouse.left = 0;
    mouse.right = 0;
    mouse.middle = 0;

    packet_index = 0;

    ps2_write_mouse(0xF6);

    if (ps2_wait_output())
        ps2_read();

    ps2_write_mouse(0xF4);

    if (ps2_wait_output())
        ps2_read();
}

void mouse_irq(void)
{
    unsigned char data;

    if (!ps2_wait_output())
        return;

    data = ps2_read();

    if (packet_index == 0 && !(data & 0x08))
        return;

    packet[packet_index++] = data;

    if (packet_index < 3)
        return;

    packet_index = 0;

    mouse.left = packet[0] & 1;
    mouse.right = (packet[0] >> 1) & 1;
    mouse.middle = (packet[0] >> 2) & 1;

    if (!(packet[0] & 0x40))
        mouse.x += (signed char)packet[1];

    if (!(packet[0] & 0x80))
        mouse.y -= (signed char)packet[2];

    if (mouse.x < 0)
        mouse.x = 0;

    if (mouse.x > bound_x)
        mouse.x = bound_x;

    if (mouse.y < 0)
        mouse.y = 0;

    if (mouse.y > bound_y)
        mouse.y = bound_y;
}

mouse_state_t mouse_get_state(void)
{
    return mouse;
}
