#include "ui/pong.h"
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
