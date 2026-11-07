#include "apps/pong.h"
#include "apps/app.h"
#include "gui/gfx.h"
#include "kernel/state.h"
#include "drivers/vga.h"

void draw_pong(void)
{
    char score_text[12];
    char reversed[12];

    int value;
    int pos;
    int count;

    vga_clear(0x00);

    vga_print("PONG", 0, 0, 0x0F);

    value = score;
    pos = 0;

    if(value == 0)
    {
        score_text[pos++] = '0';
    }
    else
    {
        count = 0;

        while(value > 0 && count < 11)
        {
            reversed[count++] =
                '0' + (value % 10);

            value /= 10;
        }

        while(count > 0)
            score_text[pos++] =
                reversed[--count];
    }

    score_text[pos] = '\0';

    vga_print("Score:", 0, 70 - pos, 0x0F);
    vga_print(score_text, 0, 76 - pos, 0x0F);

    vga_put('#', paddle_y, 2, 0x0F);
    vga_put('#', paddle_y + 1, 2, 0x0F);
    vga_put('#', paddle_y + 2, 2, 0x0F);

    vga_put('O', ball_y, ball_x, 0x0F);

    vga_cursor_hide();
}

void update_pong(void)
{
    ball_x += ball_dx;
    ball_y += ball_dy;

    if(ball_y <= 1)
    {
        ball_y = 1;
        ball_dy = 1;
    }

    if(ball_y >= 24)
    {
        ball_y = 24;
        ball_dy = -1;
    }

    if(ball_x <= 3 && ball_dx < 0)
    {
        if(ball_y >= paddle_y &&
           ball_y <= paddle_y + 2)
        {
            ball_x = 3;
            ball_dx = 1;
            score++;
        }
        else
        {
            ball_x = 70;
            ball_y = 12;
            ball_dx = -1;
            ball_dy = 1;
            score = 0;
        }
    }

    if(ball_x >= 79)
    {
        ball_x = 79;
        ball_dx = -1;
    }

    draw_pong();
}

void pong_key(int key)
{
    if(key == 'w' && paddle_y > 1)
        paddle_y--;
    if(key == 's' && paddle_y < 22)
        paddle_y++;
}

static void pong_icon(int x, int y, int s)
{
        app_icon_box(x, y, s, 2, 4, 28, 24, GFX_RGB(27, 27, 27));
        app_icon_box(x, y, s, 6, 10, 3, 10, APP_WHITE);
        app_icon_box(x, y, s, 23, 12, 3, 10, APP_WHITE);
        app_icon_box(x, y, s, 15, 14, 3, 3, APP_WHITE);
}

const app_t pong_app =
{
    SCREEN_PONG, "Pong", "Pong",
    0, 0,
    pong_key, pong_icon,
    update_pong, 5, APP_TICK_FOCUS,
    0, draw_pong
};
