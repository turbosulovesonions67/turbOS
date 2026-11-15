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
/* HERE IT GOESSSSSS!!!!!!!!!!!!!!!!!!!!*/
#define PG_BG GFX_RGB(16, 18, 24)
#define PG_BAR GFX_RGB(45, 45, 45)
#define PG_LINE GFX_RGB(70, 70, 70)
#define PG_TEXT GFX_RGB(210, 210, 210)
#define PG_DIM GFX_RGB(140, 140, 140)
#define PG_BALL GFX_RGB(80, 200, 220)
#define PG_TOOL_H 38
#define PG_COLS 80
#define PG_ROWS 24

static void number_text(int value, char *out)
{
    char tmp[12];
    int n = 0;
    int i = 0;

    do
    {
        tmp[n++] = (char)('0' + value % 10);
        value /= 10;
    } while(value > 0);

    while(n > 0)
        out[i++] = tmp[--n];

    out[i] = 0;
}

static void pong_field(int x, int y, int w, int h,
                       int *ox, int *oy, int *cw, int *ch)
{
    *cw = w / PG_COLS;
    *ch = (h - PG_TOOL_H) / PG_ROWS;
    *ox = x + (w - *cw * PG_COLS) / 2;
    *oy = y + PG_TOOL_H + (h - PG_TOOL_H - *ch * PG_ROWS) / 2;
}

static void pong_draw(int x, int y, int w, int h)
{
    char num[12];
    int ox;
    int oy;
    int cw;
    int ch;
    int side;
    int right = x + w - 16;
    int r;

    pong_field(x, y, w, h, &ox, &oy, &cw, &ch);
    side = cw < ch ? cw : ch;

    gfx_fill_rect(x, y, w, h, PG_BG);
    gfx_fill_rect(x, y, w, PG_TOOL_H, PG_BAR);
    gfx_hline(x, y + PG_TOOL_H - 1, w, PG_LINE);
    gfx_draw_text(x + 16, y + 11, "PONG", PG_TEXT);
    gfx_draw_text(x + 32 + gfx_text_width("PONG"), y + 11,
                  "W / S or click to move", PG_DIM);

    number_text(score, num);
    gfx_draw_text(right - gfx_text_width(num), y + 11, num, PG_TEXT);
    gfx_draw_text(right - gfx_text_width(num) - gfx_text_width("Score  "),
                  y + 11, "Score", PG_DIM);

    for(r = 0; r < PG_ROWS; r += 2)
        gfx_fill_rect(ox + PG_COLS / 2 * cw - 1, oy + r * ch + ch / 4,
                      2, ch / 2, PG_LINE);

    gfx_fill_rect(ox + 2 * cw, oy + (paddle_y - 1) * ch, cw, 3 * ch, PG_TEXT);
    gfx_fill_rect(ox + ball_x * cw + (cw - side) / 2,
                  oy + (ball_y - 1) * ch + (ch - side) / 2,
                  side, side, PG_BALL);
}

static void pong_mouse(int x, int y, int w, int h, int mx, int my)
{
    int ox;
    int oy;
    int cw;
    int ch;

    (void)mx;

    if(my < y + PG_TOOL_H)
        return;

    pong_field(x, y, w, h, &ox, &oy, &cw, &ch);
    paddle_y = (my - oy) / ch;

    if(paddle_y < 1)
        paddle_y = 1;

    if(paddle_y > 22)
        paddle_y = 22;
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
    pong_draw, pong_mouse,
    pong_key, pong_icon,
    update_pong, 5, APP_TICK_FOCUS,
    0, draw_pong
};
