#include "gui/desktop.h"
#include "gui/gfx.h"
#include "kernel/state.h"
#include "kernel/accounts.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/pit.h"
#include "time/rtc.h"
#include "power/power.h"
#include "ui/home.h"
#include "ui/saver.h"
#include "ui/editor.h"
#include "ui/terminal.h"
#include "ui/filemanager.h"
#include "ui/pong.h"
#define APP_COUNT 6
#define TASKBAR_H     40
#define TITLE_H       32
#define CAPTION_W     46
#define CLIENT_W      (VGA_WIDTH * 8)
#define CLIENT_H      (VGA_HEIGHT * 16)
#define START_BTN_W   48
#define TB_BTN_W      48
#define CLOCK_W       104
#define SHOW_W        6
#define MENU_W        320
#define MENU_H        344
#define MENU_RAIL     48
#define MENU_HEAD     40
#define MENU_ITEM_H   48
#define FLY_W         168
#define FLY_ITEM_H    40
#define FLY_H         (FLY_ITEM_H * 2 + 8)
#define ICON_CELL_W   76
#define ICON_CELL_H   84
#define DOUBLE_CLICK_TICKS 50        
#define BLINK_TICKS        50
#define C_DESKTOP     GFX_RGB(0, 99, 177)
#define C_ACCENT      GFX_RGB(0, 120, 215)
#define C_ACCENT_LT   GFX_RGB(118, 185, 237)
#define C_WHITE       GFX_RGB(255, 255, 255)
#define C_BLACK       GFX_RGB(0, 0, 0)
#define C_TITLE_BG    GFX_RGB(255, 255, 255)
#define C_TITLE_TXT   GFX_RGB(0, 0, 0)
#define C_TITLE_OFF   GFX_RGB(153, 153, 153)
#define C_BORDER_OFF  GFX_RGB(170, 170, 170)
#define C_CAP_HOVER   GFX_RGB(229, 229, 229)
#define C_CLOSE_HOVER GFX_RGB(232, 17, 35)
#define C_TASKBAR     GFX_RGB(19, 19, 19)
#define C_TB_HOVER    GFX_RGB(51, 51, 51)
#define C_TB_ACTIVE   GFX_RGB(62, 62, 62)
#define C_TB_LINE     GFX_RGB(176, 176, 176)
#define C_MENU_LIST   GFX_RGB(31, 31, 31)
#define C_MENU_RAIL   GFX_RGB(24, 24, 24)
#define C_MENU_HOVER  GFX_RGB(51, 51, 51)
#define C_FLY_BG      GFX_RGB(43, 43, 43)
#define C_FLY_BORDER  GFX_RGB(74, 74, 74)
#define C_FLY_HOVER   GFX_RGB(63, 63, 63)
#define C_GRAY_TEXT   GFX_RGB(160, 160, 160)
enum
{
    ID_NONE       = 0,
    ID_DESKTOP    = 1,
    ID_TASKBAR    = 2,
    ID_START      = 3,
    ID_CLOCK      = 4,
    ID_SHOW       = 5,
    ID_MENU_BLANK = 6,
    ID_POWER      = 7,
    ID_FLY        = 10,      
    ID_MENU       = 20,      
    ID_ICON       = 30,      
    ID_TBBTN      = 40,      
    ID_TITLE      = 50,      
    ID_CLIENT     = 60,      
    ID_CAPTION    = 100      
};
typedef struct
{
    int open;
    int minimized;
    int maximized;
    int x;                   
    int y;
    int rx;                  
    int ry;
} window_t;
static window_t win[APP_COUNT];
static int zorder[APP_COUNT];            
static int scr_w;
static int scr_h;
static int work_h;                       
static int focus = -1;                   
static int screen_seen;                  
static int start_open;
static int power_open;
static int selected_icon = -1;
static int hover = ID_NONE;
static int drag_app = -1;
static int drag_dx;
static int drag_dy;
static int prev_left;
static int last_click_id = -1;
static unsigned int last_click_tick;
static int dirty_valid;
static int dirty_x0;
static int dirty_y0;
static int dirty_x1;
static int dirty_y1;
static unsigned int seen_version[APP_COUNT];
static unsigned int seen_epoch;
static int blink_on = 1;
static int cursor_drawn;
static int cursor_x;
static int cursor_y;
static int clock_valid;
static unsigned int last_clock_poll;
static unsigned char last_min;
static unsigned char last_sec;
static char time_text[6];                
static char date_text[11];               
static int desktop_ready;
static int imin(int a, int b)
{
    return a < b ? a : b;
}
static int imax(int a, int b)
{
    return a > b ? a : b;
}
static int in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && py >= y && px < x + w && py < y + h;
}
static const char *app_title(int a)
{
    switch(a)
    {
        case SCREEN_HOME:        return "Home";
        case SCREEN_SAVER:       return "Screen Saver";
        case SCREEN_EDITOR:      return "Text Editor";
        case SCREEN_PONG:        return "Pong";
        case SCREEN_TERMINAL:    return "Terminal";
        case SCREEN_FILEMANAGER: return "File Manager";
    }
    return "";
}
static const char *app_label(int a)
{
    switch(a)
    {
        case SCREEN_HOME:        return "Home";
        case SCREEN_SAVER:       return "Saver";
        case SCREEN_EDITOR:      return "Editor";
        case SCREEN_PONG:        return "Pong";
        case SCREEN_TERMINAL:    return "Terminal";
        case SCREEN_FILEMANAGER: return "Files";
    }
    return "";
}
static int app_in_order(int position)
{
    switch(position)
    {
        case 0: return SCREEN_HOME;
        case 1: return SCREEN_TERMINAL;
        case 2: return SCREEN_FILEMANAGER;
        case 3: return SCREEN_EDITOR;
        case 4: return SCREEN_PONG;
        default: return SCREEN_SAVER;
    }
}
static int win_visible(int a)
{
    return win[a].open && !win[a].minimized;
}
static int win_x(int a)
{
    return win[a].maximized ? 0 : win[a].x;
}
static int win_y(int a)
{
    return win[a].maximized ? 0 : win[a].y;
}
static int win_w(int a)
{
    return win[a].maximized ? scr_w : CLIENT_W + 2;
}
static int win_h(int a)
{
    return win[a].maximized ? work_h : CLIENT_H + TITLE_H + 2;
}
static int taskbar_y(void)
{
    return scr_h - TASKBAR_H;
}
static int menu_y(void)
{
    return imax(0, taskbar_y() - MENU_H);
}
static int tb_button_x(int a)
{
    int i;
    int k = 0;
    if(!win[a].open)
        return -1;
    for(i = 0; i < a; i++)
        if(win[i].open)
            k++;
    return START_BTN_W + k * TB_BTN_W;
}
static void caption_rect(int a, int btn, int *x, int *y)
{
    int bw = win[a].maximized ? 0 : 1;
    *x = win_x(a) + win_w(a) - bw - (3 - btn) * CAPTION_W;
    *y = win_y(a) + bw;
}
static void dirty_add(int x, int y, int w, int h)
{
    int x1 = x + w;
    int y1 = y + h;
    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;
    if(x1 > scr_w)
        x1 = scr_w;
    if(y1 > scr_h)
        y1 = scr_h;
    if(x >= x1 || y >= y1)
        return;
    if(!dirty_valid)
    {
        dirty_x0 = x;
        dirty_y0 = y;
        dirty_x1 = x1;
        dirty_y1 = y1;
        dirty_valid = 1;
        return;
    }
    dirty_x0 = imin(dirty_x0, x);
    dirty_y0 = imin(dirty_y0, y);
    dirty_x1 = imax(dirty_x1, x1);
    dirty_y1 = imax(dirty_y1, y1);
}
static void dirty_all(void)
{
    dirty_add(0, 0, scr_w, scr_h);
}
static void dirty_win(int a)
{
    dirty_add(win_x(a), win_y(a), win_w(a), win_h(a));
}
static void dirty_taskbar(void)
{
    dirty_add(0, taskbar_y(), scr_w, TASKBAR_H);
}
static void dirty_menu(void)
{
    dirty_add(0, menu_y(), MENU_W, MENU_H);
    dirty_add(0, taskbar_y(), START_BTN_W, TASKBAR_H);
}
static void dirty_clock(void)
{
    dirty_add(scr_w - SHOW_W - CLOCK_W, taskbar_y(), CLOCK_W, TASKBAR_H);
}
static int sc(int v, int s)
{
    return (v * s + 16) / 32;
}
static void box(int x, int y, int s,
                int ax, int ay, int aw, int ah, uint32_t c)
{
    int x0 = x + sc(ax, s);
    int y0 = y + sc(ay, s);
    int x1 = x + sc(ax + aw, s);
    int y1 = y + sc(ay + ah, s);
    if(x1 <= x0)
        x1 = x0 + 1;
    if(y1 <= y0)
        y1 = y0 + 1;
    gfx_fill_rect(x0, y0, x1 - x0, y1 - y0, c);
}
static void draw_app_icon(int a, int x, int y, int s)
{
    int i;
    switch(a)
    {
    case SCREEN_HOME:
        for(i = 0; i < 13; i++)
            box(x, y, s, 15 - i, 3 + i, 2 + 2 * i, 1, GFX_RGB(242, 80, 34));
        box(x, y, s, 6, 16, 20, 13, C_WHITE);
        box(x, y, s, 13, 20, 6, 9, C_ACCENT);
        break;
    case SCREEN_TERMINAL:
        box(x, y, s, 2, 5, 28, 22, GFX_RGB(31, 31, 31));
        box(x, y, s, 2, 5, 28, 4, GFX_RGB(230, 230, 230));
        gfx_line(x + sc(7, s), y + sc(12, s),
                 x + sc(12, s), y + sc(16, s), GFX_RGB(22, 198, 12));
        gfx_line(x + sc(12, s), y + sc(16, s),
                 x + sc(7, s), y + sc(20, s), GFX_RGB(22, 198, 12));
        if(s >= 24)
        {
            gfx_line(x + sc(7, s) + 1, y + sc(12, s),
                     x + sc(12, s) + 1, y + sc(16, s), GFX_RGB(22, 198, 12));
            gfx_line(x + sc(12, s) + 1, y + sc(16, s),
                     x + sc(7, s) + 1, y + sc(20, s), GFX_RGB(22, 198, 12));
        }
        box(x, y, s, 15, 20, 8, 2, GFX_RGB(204, 204, 204));
        break;
    case SCREEN_FILEMANAGER:
        box(x, y, s, 2, 6, 13, 6, GFX_RGB(227, 168, 30));
        box(x, y, s, 2, 10, 28, 17, GFX_RGB(255, 211, 90));
        box(x, y, s, 2, 15, 28, 12, GFX_RGB(255, 224, 138));
        break;
    case SCREEN_EDITOR:
        box(x, y, s, 6, 3, 20, 26, C_WHITE);
        box(x, y, s, 6, 3, 20, 1, GFX_RGB(200, 200, 200));
        box(x, y, s, 10, 8, 8, 2, C_ACCENT);
        box(x, y, s, 10, 13, 12, 2, GFX_RGB(150, 160, 170));
        box(x, y, s, 10, 18, 12, 2, GFX_RGB(150, 160, 170));
        box(x, y, s, 10, 23, 9, 2, GFX_RGB(150, 160, 170));
        break;
    case SCREEN_PONG:
        box(x, y, s, 2, 4, 28, 24, GFX_RGB(27, 27, 27));
        box(x, y, s, 6, 10, 3, 10, C_WHITE);
        box(x, y, s, 23, 12, 3, 10, C_WHITE);
        box(x, y, s, 15, 14, 3, 3, C_WHITE);
        break;
    case SCREEN_SAVER:
        box(x, y, s, 2, 4, 28, 24, GFX_RGB(31, 45, 99));
        gfx_fill_circle(x + sc(15, s), y + sc(16, s), sc(7, s),
                        GFX_RGB(255, 216, 107));
        gfx_fill_circle(x + sc(19, s), y + sc(14, s), sc(6, s),
                        GFX_RGB(31, 45, 99));
        box(x, y, s, 24, 8, 2, 2, C_WHITE);
        box(x, y, s, 8, 9, 2, 2, C_WHITE);
        box(x, y, s, 24, 22, 2, 2, C_WHITE);
        break;
    }
}
static void raise_window(int a)
{
    int i;
    int pos = 0;
    for(i = 0; i < APP_COUNT; i++)
        if(zorder[i] == a)
            pos = i;
    for(i = pos; i < APP_COUNT - 1; i++)
        zorder[i] = zorder[i + 1];
    zorder[APP_COUNT - 1] = a;
}
static int top_visible(void)
{
    int i;
    for(i = APP_COUNT - 1; i >= 0; i--)
        if(win_visible(zorder[i]))
            return zorder[i];
    return -1;
}
static void set_focus(int a)
{
    int old = focus;
    focus = a;
    if(a >= 0)
    {
        current_screen = a;
        screen_seen = a;
    }
    if(old != a)
    {
        if(old >= 0)
            dirty_win(old);
        if(a >= 0)
            dirty_win(a);
        dirty_taskbar();
    }
}
static void app_redraw(int a)
{
    int saved = current_screen;
    current_screen = a;
    switch(a)
    {
        case SCREEN_HOME:        draw_home();         break;
        case SCREEN_SAVER:       draw_saver();        break;
        case SCREEN_EDITOR:      draw_editor();       break;
        case SCREEN_PONG:        draw_pong();         break;
        case SCREEN_TERMINAL:    draw_terminal();     break;
        case SCREEN_FILEMANAGER: draw_file_manager(); break;
    }
    current_screen = saved;
}
static void place_default(int a)
{
    int x = 110 + a * 28;
    int y = 16 + a * 24;
    if(x + CLIENT_W + 2 > scr_w)
        x = scr_w - (CLIENT_W + 2);
    if(y + CLIENT_H + TITLE_H + 2 > work_h)
        y = work_h - (CLIENT_H + TITLE_H + 2);
    win[a].x = imax(x, 0);
    win[a].y = imax(y, 0);
}
static void launch(int a)
{
    if(!win[a].open)
    {
        win[a].open = 1;
        win[a].maximized = 0;
        place_default(a);
    }
    win[a].minimized = 0;
    raise_window(a);
    set_focus(a);
    app_redraw(a);
    dirty_win(a);
    dirty_taskbar();
}
static void close_window(int a)
{
    dirty_win(a);
    if(win[a].maximized)
    {
        win[a].maximized = 0;
        win[a].x = win[a].rx;
        win[a].y = win[a].ry;
    }
    win[a].open = 0;
    win[a].minimized = 0;
    if(focus == a)
    {
        focus = -1;
        set_focus(top_visible());
    }
    dirty_taskbar();
}
static void minimize_window(int a)
{
    dirty_win(a);
    win[a].minimized = 1;
    if(focus == a)
    {
        focus = -1;
        set_focus(top_visible());
    }
    dirty_taskbar();
}
static void toggle_maximize(int a)
{
    dirty_win(a);
    if(win[a].maximized)
    {
        win[a].maximized = 0;
        win[a].x = win[a].rx;
        win[a].y = win[a].ry;
    }
    else
    {
        win[a].rx = win[a].x;
        win[a].ry = win[a].y;
        win[a].maximized = 1;
    }
    dirty_win(a);
}
static void move_window(int a, int nx, int ny)
{
    if(win[a].maximized)
        return;
    nx = imax(nx, 80 - win_w(a));
    nx = imin(nx, scr_w - 80);
    ny = imax(ny, 0);
    ny = imin(ny, work_h - TITLE_H);
    if(nx == win[a].x && ny == win[a].y)
        return;
    dirty_win(a);
    win[a].x = nx;
    win[a].y = ny;
    dirty_win(a);
}
static void show_desktop(void)
{
    int i;
    int any = 0;
    for(i = 0; i < APP_COUNT; i++)
        if(win_visible(i))
            any = 1;
    for(i = 0; i < APP_COUNT; i++)
    {
        if(!win[i].open)
            continue;
        dirty_win(i);
        win[i].minimized = any;
    }
    if(any)
    {
        focus = -1;
    }
    else
    {
        int top = top_visible();
        focus = -1;
        set_focus(top);
    }
    dirty_taskbar();
}
static void taskbar_click(int a)
{
    if(win[a].minimized)
    {
        win[a].minimized = 0;
        raise_window(a);
        set_focus(a);
        dirty_win(a);
        dirty_taskbar();
    }
    else if(focus == a)
    {
        minimize_window(a);
    }
    else
    {
        raise_window(a);
        set_focus(a);
        dirty_win(a);
    }
}
static void sync_focus(void)
{
    int a = current_screen;
    if(a == screen_seen)
        return;
    screen_seen = a;
    if(a < 0 || a >= APP_COUNT)
        return;
    if(!win_visible(a))
    {
        if(!win[a].open)
        {
            win[a].open = 1;
            win[a].maximized = 0;
            place_default(a);
        }
        win[a].minimized = 0;
        app_redraw(a);
    }
    raise_window(a);
    set_focus(a);
    dirty_win(a);
    dirty_taskbar();
}
static void set_start(int open)
{
    if(open == start_open && (open || !power_open))
        return;
    start_open = open;
    if(!open)
        power_open = 0;
    dirty_menu();
}
static int hit(int mx, int my)
{
    int i;
    int ty = taskbar_y();
    int my0 = menu_y();
    if(power_open &&
       in_rect(mx, my, MENU_RAIL, my0 + MENU_H - FLY_H, FLY_W, FLY_H))
    {
        int item = (my - (my0 + MENU_H - FLY_H + 4)) / FLY_ITEM_H;
        if(item >= 0 && item < 2)
            return ID_FLY + item;
        return ID_MENU_BLANK;
    }
    if(start_open && in_rect(mx, my, 0, my0, MENU_W, MENU_H))
    {
        if(mx < MENU_RAIL)
        {
            if(my >= my0 + MENU_H - 48)
                return ID_POWER;
            return ID_MENU_BLANK;
        }
        i = (my - (my0 + MENU_HEAD)) / MENU_ITEM_H;
        if(my >= my0 + MENU_HEAD && i >= 0 && i < APP_COUNT)
            return ID_MENU + i;
        return ID_MENU_BLANK;
    }
    if(my >= ty)
    {
        if(mx < START_BTN_W)
            return ID_START;
        if(mx >= scr_w - SHOW_W)
            return ID_SHOW;
        if(mx >= scr_w - SHOW_W - CLOCK_W)
            return ID_CLOCK;
        for(i = 0; i < APP_COUNT; i++)
        {
            int bx = tb_button_x(i);
            if(bx >= 0 && mx >= bx && mx < bx + TB_BTN_W)
                return ID_TBBTN + i;
        }
        return ID_TASKBAR;
    }
    for(i = APP_COUNT - 1; i >= 0; i--)
    {
        int a = zorder[i];
        int bw;
        if(!win_visible(a))
            continue;
        if(!in_rect(mx, my, win_x(a), win_y(a), win_w(a), win_h(a)))
            continue;
        bw = win[a].maximized ? 0 : 1;
        if(my < win_y(a) + bw + TITLE_H)
        {
            int btn;
            for(btn = 0; btn < 3; btn++)
            {
                int cx;
                int cy;
                caption_rect(a, btn, &cx, &cy);
                if(in_rect(mx, my, cx, cy, CAPTION_W, TITLE_H))
                    return ID_CAPTION + a * 4 + btn;
            }
            return ID_TITLE + a;
        }
        return ID_CLIENT + a;
    }
    for(i = 0; i < APP_COUNT; i++)
        if(in_rect(mx, my, 8, 8 + i * ICON_CELL_H,
                   ICON_CELL_W, ICON_CELL_H))
            return ID_ICON + i;
    return ID_DESKTOP;
}
static void hover_dirty(int id)
{
    if(id >= ID_CAPTION)
    {
        int a = (id - ID_CAPTION) / 4;
        int btn = (id - ID_CAPTION) % 4;
        int x;
        int y;
        caption_rect(a, btn, &x, &y);
        dirty_add(x, y, CAPTION_W, TITLE_H);
    }
    else if(id >= ID_TBBTN && id < ID_TITLE)
    {
        int x = tb_button_x(id - ID_TBBTN);
        if(x >= 0)
            dirty_add(x, taskbar_y(), TB_BTN_W, TASKBAR_H);
    }
    else if(id >= ID_ICON && id < ID_TBBTN)
    {
        dirty_add(8, 8 + (id - ID_ICON) * ICON_CELL_H,
                  ICON_CELL_W, ICON_CELL_H);
    }
    else if(id >= ID_MENU && id < ID_ICON)
    {
        dirty_add(MENU_RAIL,
                  menu_y() + MENU_HEAD + (id - ID_MENU) * MENU_ITEM_H,
                  MENU_W - MENU_RAIL, MENU_ITEM_H);
    }
    else if(id >= ID_FLY && id < ID_MENU)
    {
        dirty_add(MENU_RAIL, menu_y() + MENU_H - FLY_H, FLY_W, FLY_H);
    }
    else if(id == ID_START)
    {
        dirty_add(0, taskbar_y(), START_BTN_W, TASKBAR_H);
    }
    else if(id == ID_POWER)
    {
        dirty_add(0, menu_y() + MENU_H - 48, MENU_RAIL, 48);
    }
    else if(id == ID_CLOCK)
    {
        dirty_clock();
    }
    else if(id == ID_SHOW)
    {
        dirty_add(scr_w - SHOW_W, taskbar_y(), SHOW_W, TASKBAR_H);
    }
}
static void draw_desktop_icons(void)
{
    int p;
    for(p = 0; p < APP_COUNT; p++)
    {
        int a = app_in_order(p);
        int cx = 8;
        int cy = 8 + p * ICON_CELL_H;
        const char *label = app_label(a);
        int tx = cx + (ICON_CELL_W - gfx_text_width(label)) / 2;
        if(selected_icon == p)
        {
            gfx_blend_rect(cx, cy, ICON_CELL_W, ICON_CELL_H, C_WHITE, 50);
            gfx_rect_outline(cx, cy, ICON_CELL_W, ICON_CELL_H, C_ACCENT_LT);
        }
        else if(hover == ID_ICON + p)
        {
            gfx_blend_rect(cx, cy, ICON_CELL_W, ICON_CELL_H, C_WHITE, 25);
        }
        draw_app_icon(a, cx + (ICON_CELL_W - 32) / 2, cy + 8, 32);
        gfx_draw_text(tx + 1, cy + 47, label, C_BLACK);
        gfx_draw_text(tx, cy + 46, label, C_WHITE);
    }
}
static void draw_caption_glyph(int btn, int maximized, int cx, int cy,
                               uint32_t color, uint32_t bg)
{
    if(btn == 0)
    {
        gfx_hline(cx - 5, cy, 10, color);
    }
    else if(btn == 1)
    {
        if(maximized)
        {
            gfx_rect_outline(cx - 3, cy - 5, 8, 8, color);
            gfx_fill_rect(cx - 5, cy - 3, 8, 8, bg);
            gfx_rect_outline(cx - 5, cy - 3, 8, 8, color);
        }
        else
        {
            gfx_rect_outline(cx - 5, cy - 5, 10, 10, color);
        }
    }
    else
    {
        gfx_line(cx - 5, cy - 5, cx + 5, cy + 5, color);
        gfx_line(cx + 5, cy - 5, cx - 5, cy + 5, color);
    }
}
static void draw_caption_buttons(int a, int active)
{
    int btn;
    for(btn = 0; btn < 3; btn++)
    {
        int x;
        int y;
        int hovered = (hover == ID_CAPTION + a * 4 + btn);
        uint32_t bg = C_TITLE_BG;
        uint32_t fg = active ? C_TITLE_TXT : C_TITLE_OFF;
        caption_rect(a, btn, &x, &y);
        if(hovered)
        {
            bg = (btn == 2) ? C_CLOSE_HOVER : C_CAP_HOVER;
            if(btn == 2)
                fg = C_WHITE;
            else
                fg = C_TITLE_TXT;
            gfx_fill_rect(x, y, CAPTION_W, TITLE_H, bg);
        }
        draw_caption_glyph(btn, win[a].maximized,
                           x + CAPTION_W / 2, y + TITLE_H / 2, fg, bg);
    }
}
static void draw_client(int a, int cx, int cy, int cw, int ch, int active)
{
    int gx = cx + (cw - CLIENT_W) / 2;
    int gy = cy + (ch - CLIENT_H) / 2;
    int x0;
    int y0;
    int x1;
    int y1;
    int vx0;
    int vy0;
    int vx1;
    int vy1;
    int row;
    int col;
    int r0;
    int r1;
    int c0;
    int c1;
    int crow;
    int ccol;
    if(cw != CLIENT_W || ch != CLIENT_H)
    {
        unsigned short first = vga_surface_cell(a, 0, 0);
        gfx_fill_rect(cx, cy, cw, ch, vga_color((first >> 12) & 0x0F));
    }
    gfx_clip_get(&x0, &y0, &x1, &y1);
    vx0 = imax(x0, gx);
    vy0 = imax(y0, gy);
    vx1 = imin(x1, gx + CLIENT_W);
    vy1 = imin(y1, gy + CLIENT_H);
    if(vx0 >= vx1 || vy0 >= vy1)
        return;
    c0 = (vx0 - gx) / 8;
    c1 = (vx1 - gx + 7) / 8;
    r0 = (vy0 - gy) / 16;
    r1 = (vy1 - gy + 15) / 16;
    for(row = r0; row < r1; row++)
    {
        for(col = c0; col < c1; col++)
        {
            unsigned short cell = vga_surface_cell(a, row, col);
            gfx_draw_cell(gx + col * 8, gy + row * 16,
                          (char)(cell & 0xFF),
                          vga_color((cell >> 8) & 0x0F),
                          vga_color((cell >> 12) & 0x0F));
        }
    }
    if(active && blink_on && vga_surface_cursor(a, &crow, &ccol))
    {
        unsigned short cell = vga_surface_cell(a, crow, ccol);
        gfx_fill_rect(gx + ccol * 8, gy + crow * 16 + 13, 8, 2,
                      vga_color((cell >> 8) & 0x0F));
    }
}
static void draw_window(int a)
{
    int wx = win_x(a);
    int wy = win_y(a);
    int ww = win_w(a);
    int wh = win_h(a);
    int bw = win[a].maximized ? 0 : 1;
    int active = (focus == a);
    int x0;
    int y0;
    int x1;
    int y1;
    gfx_clip_get(&x0, &y0, &x1, &y1);
    if(wx >= x1 || wy >= y1 || wx + ww <= x0 || wy + wh <= y0)
        return;
    if(bw)
        gfx_rect_outline(wx, wy, ww, wh,
                         active ? C_ACCENT : C_BORDER_OFF);
    gfx_fill_rect(wx + bw, wy + bw, ww - 2 * bw, TITLE_H, C_TITLE_BG);
    draw_app_icon(a, wx + bw + 8, wy + bw + 8, 16);
    gfx_draw_text(wx + bw + 32, wy + bw + 8, app_title(a),
                  active ? C_TITLE_TXT : C_TITLE_OFF);
    draw_caption_buttons(a, active);
    draw_client(a, wx + bw, wy + bw + TITLE_H,
                ww - 2 * bw, wh - 2 * bw - TITLE_H, active);
}
static void draw_logo(int x, int y, int size, uint32_t c)
{
    int yy;
    int y0 = y + size * 4 / 32;
    int y1 = y + size * 28 / 32;
    int w = size * 4 / 32 < 1 ? 1 : size * 4 / 32;
    gfx_fill_rect(x + size * 8 / 32, y + size * 4 / 32, size * 5 / 32, size * 23 / 32, c);
    gfx_fill_rect(x + size * 3 / 32, y + size * 10 / 32, size * 13 / 32, size * 4 / 32, c);
    gfx_fill_rect(x + size * 8 / 32, y + size * 23 / 32, size * 9 / 32, size * 4 / 32, c);
    gfx_fill_rect(x + size * 13 / 32, y + size * 20 / 32, size * 4 / 32, size * 7 / 32, c);
    for(yy = y0; yy < y1; yy++)
        gfx_fill_rect(x + size * 19 / 32 + (yy - y0) * (size * 9 / 32) / (y1 - y0), yy, w, 1, c);
}
static void draw_taskbar(void)
{
    int ty = taskbar_y();
    int a;
    int x;
    int cx;
    gfx_fill_rect(0, ty, scr_w, TASKBAR_H, C_TASKBAR);
    if(start_open)
        gfx_fill_rect(0, ty, START_BTN_W, TASKBAR_H, C_TB_ACTIVE);
    else if(hover == ID_START)
        gfx_fill_rect(0, ty, START_BTN_W, TASKBAR_H, C_TB_HOVER);
    draw_logo(12, ty + 8, 24, C_WHITE);
    for(a = 0; a < APP_COUNT; a++)
    {
        x = tb_button_x(a);
        if(x < 0)
            continue;
        if(focus == a)
            gfx_fill_rect(x, ty, TB_BTN_W, TASKBAR_H, C_TB_ACTIVE);
        else if(hover == ID_TBBTN + a)
            gfx_fill_rect(x, ty, TB_BTN_W, TASKBAR_H, C_TB_HOVER);
        draw_app_icon(a, x + (TB_BTN_W - 20) / 2, ty + 10, 20);
        if(focus == a)
            gfx_fill_rect(x, ty + TASKBAR_H - 3, TB_BTN_W, 3, C_ACCENT_LT);
        else
            gfx_fill_rect(x + (TB_BTN_W - 16) / 2, ty + TASKBAR_H - 3,
                          16, 3, C_TB_LINE);
    }
    cx = scr_w - SHOW_W - CLOCK_W;
    if(hover == ID_CLOCK)
        gfx_fill_rect(cx, ty, CLOCK_W, TASKBAR_H, C_TB_HOVER);
    gfx_draw_text(cx + (CLOCK_W - gfx_text_width(time_text)) / 2,
                  ty + 4, time_text, C_WHITE);
    gfx_draw_text(cx + (CLOCK_W - gfx_text_width(date_text)) / 2,
                  ty + 20, date_text, C_WHITE);
    if(hover == ID_SHOW)
        gfx_fill_rect(scr_w - SHOW_W, ty, SHOW_W, TASKBAR_H, C_TB_HOVER);
    gfx_vline(scr_w - SHOW_W, ty + 6, TASKBAR_H - 12, GFX_RGB(90, 90, 90));
}
static void draw_power_glyph(int cx, int cy, uint32_t fg, uint32_t bg)
{
    gfx_fill_circle(cx, cy + 1, 8, fg);
    gfx_fill_circle(cx, cy + 1, 6, bg);
    gfx_fill_rect(cx - 3, cy - 9, 7, 7, bg);
    gfx_fill_rect(cx, cy - 9, 2, 10, fg);
}
static void draw_start_menu(void)
{
    int my0 = menu_y();
    int p;
    int bottom = my0 + MENU_H;
    char initial[2];
    if(!start_open)
        return;
    gfx_fill_rect(0, my0, MENU_W, MENU_H, C_MENU_LIST);
    gfx_fill_rect(0, my0, MENU_RAIL, MENU_H, C_MENU_RAIL);
    gfx_fill_rect(16, my0 + 14, 16, 2, C_WHITE);
    gfx_fill_rect(16, my0 + 20, 16, 2, C_WHITE);
    gfx_fill_rect(16, my0 + 26, 16, 2, C_WHITE);
    initial[0] = (username && username[0]) ? username[0] : '?';
    initial[1] = 0;
    if(initial[0] >= 'a' && initial[0] <= 'z')
        initial[0] = (char)(initial[0] - 'a' + 'A');
    gfx_fill_circle(MENU_RAIL / 2, bottom - 72, 11, GFX_RGB(90, 90, 90));
    gfx_draw_char(MENU_RAIL / 2 - 4, bottom - 80, initial[0], C_WHITE);
    if(hover == ID_POWER || power_open)
        gfx_fill_rect(0, bottom - 48, MENU_RAIL, 48, C_MENU_HOVER);
    draw_power_glyph(MENU_RAIL / 2, bottom - 24, C_WHITE,
                     (hover == ID_POWER || power_open) ?
                     C_MENU_HOVER : C_MENU_RAIL);
    gfx_draw_text(MENU_RAIL + 16, my0 + 12, "turbOS apps", C_GRAY_TEXT);
    for(p = 0; p < APP_COUNT; p++)
    {
        int a = app_in_order(p);
        int iy = my0 + MENU_HEAD + p * MENU_ITEM_H;
        if(hover == ID_MENU + p && !power_open)
            gfx_fill_rect(MENU_RAIL, iy, MENU_W - MENU_RAIL,
                          MENU_ITEM_H, C_MENU_HOVER);
        draw_app_icon(a, MENU_RAIL + 16, iy + 12, 24);
        gfx_draw_text(MENU_RAIL + 16 + 24 + 14, iy + 16,
                      app_title(a), C_WHITE);
    }
    if(power_open)
    {
        int fx = MENU_RAIL;
        int fy = bottom - FLY_H;
        gfx_fill_rect(fx, fy, FLY_W, FLY_H, C_FLY_BG);
        gfx_rect_outline(fx, fy, FLY_W, FLY_H, C_FLY_BORDER);
        for(p = 0; p < 2; p++)
        {
            int iy = fy + 4 + p * FLY_ITEM_H;
            if(hover == ID_FLY + p)
                gfx_fill_rect(fx + 1, iy, FLY_W - 2, FLY_ITEM_H, C_FLY_HOVER);
            gfx_draw_text(fx + 16, iy + 12,
                          p == 0 ? "Shut down" : "Restart", C_WHITE);
        }
    }
}
static int render(void)
{
    int x;
    int y;
    int w;
    int h;
    int i;
    if(!dirty_valid)
        return 0;
    x = dirty_x0;
    y = dirty_y0;
    w = dirty_x1 - dirty_x0;
    h = dirty_y1 - dirty_y0;
    dirty_valid = 0;
    gfx_clip_set(x, y, w, h);
    gfx_fill_rect(0, 0, scr_w, work_h, C_DESKTOP);
    draw_desktop_icons();
    for(i = 0; i < APP_COUNT; i++)
        if(win_visible(zorder[i]))
            draw_window(zorder[i]);
    draw_taskbar();
    draw_start_menu();
    gfx_clip_reset();
    gfx_present(x, y, w, h);
    return 1;
}
static void update_cursor(int mx, int my, int force)
{
    if(cursor_drawn && (mx != cursor_x || my != cursor_y))
    {
        gfx_cursor_erase(cursor_x, cursor_y);
        cursor_drawn = 0;
    }
    if(!cursor_drawn || force)
    {
        cursor_x = mx;
        cursor_y = my;
        gfx_cursor_draw(mx, my);
        cursor_drawn = 1;
    }
}
static void system_console(void)
{
    vga_console_begin();
    accounts_system_console();
    vga_console_end();
}
static void global_key(int key)
{
    if(focus == SCREEN_EDITOR && (key == 's' || key == 'S'))
    {
        editor_save();
        return;
    }
    if(key >= '1' && key <= '6')
        launch(key - '1');
    else if(key == '9')
        power_reboot();
    else if(key == '0')
        power_shutdown();
}
static void route_key(int key)
{
    if(focus < 0)
        return;
    if(focus == SCREEN_EDITOR)
    {
        handle_editor_key(key);
    }
    else if(focus == SCREEN_TERMINAL)
    {
        handle_terminal_key(key);
    }
    else if(focus == SCREEN_FILEMANAGER)
    {
        handle_file_manager_key(key);
    }
    else if(focus == SCREEN_PONG)
    {
        if(key == 'w' && paddle_y > 1)
            paddle_y--;
        if(key == 's' && paddle_y < 22)
            paddle_y++;
    }
}
static void handle_keys(void)
{
    int key;
    while((key = keyboard_getkey()) != KEY_NONE)
    {
        if(key == KEY_CTRL_ALT_S)
        {
            system_console();
            continue;
        }
        if(key == KEY_CTRL)
        {
            ctrl_pressed = 1;
            continue;
        }
        if(ctrl_pressed)
        {
            global_key(key);
            ctrl_pressed = 0;
            sync_focus();
            continue;
        }
        if(key == KEY_ESC && start_open)
        {
            set_start(0);
            continue;
        }
        route_key(key);
        sync_focus();
    }
}
static int is_double_click(int id)
{
    int dbl = (id == last_click_id &&
               timer_ticks - last_click_tick <= DOUBLE_CLICK_TICKS);
    last_click_id = dbl ? -1 : id;
    last_click_tick = timer_ticks;
    return dbl;
}
static void on_press(int mx, int my, int id)
{
    if(id == ID_FLY)
    {
        power_shutdown();
        return;
    }
    if(id == ID_FLY + 1)
    {
        power_reboot();
        return;
    }
    if(id == ID_POWER)
    {
        power_open = !power_open;
        dirty_menu();
        return;
    }
    if(id >= ID_MENU && id < ID_ICON)
    {
        int a = app_in_order(id - ID_MENU);
        set_start(0);
        launch(a);
        return;
    }
    if(id == ID_MENU_BLANK)
    {
        if(power_open)
        {
            power_open = 0;
            dirty_menu();
        }
        return;
    }
    if(id == ID_START)
    {
        set_start(!start_open);
        return;
    }
    if(start_open)
        set_start(0);
    if(id == ID_SHOW)
    {
        show_desktop();
    }
    else if(id >= ID_TBBTN && id < ID_TITLE)
    {
        taskbar_click(id - ID_TBBTN);
    }
    else if(id >= ID_CAPTION)
    {
        int a = (id - ID_CAPTION) / 4;
        int btn = (id - ID_CAPTION) % 4;
        if(btn == 0)
            minimize_window(a);
        else if(btn == 1)
            toggle_maximize(a);
        else
            close_window(a);
    }
    else if(id >= ID_CLIENT && id < ID_CAPTION)
    {
        int a = id - ID_CLIENT;
        if(focus != a)
        {
            raise_window(a);
            set_focus(a);
            dirty_win(a);
        }
    }
    else if(id >= ID_TITLE && id < ID_CLIENT)
    {
        int a = id - ID_TITLE;
        if(focus != a)
        {
            raise_window(a);
            set_focus(a);
            dirty_win(a);
        }
        if(is_double_click(id))
        {
            toggle_maximize(a);
        }
        else if(!win[a].maximized)
        {
            drag_app = a;
            drag_dx = mx - win[a].x;
            drag_dy = my - win[a].y;
        }
    }
    else if(id >= ID_ICON && id < ID_TBBTN)
    {
        int p = id - ID_ICON;
        if(selected_icon != p)
        {
            if(selected_icon >= 0)
                hover_dirty(ID_ICON + selected_icon);
            selected_icon = p;
            hover_dirty(id);
        }
        if(is_double_click(id))
            launch(app_in_order(p));
    }
    else
    {
        if(selected_icon >= 0)
        {
            hover_dirty(ID_ICON + selected_icon);
            selected_icon = -1;
        }
    }
    (void)my;
}
static void handle_mouse(int mx, int my, int left)
{
    int id = hit(mx, my);
    if(id != hover)
    {
        hover_dirty(hover);
        hover = id;
        hover_dirty(id);
    }
    if(drag_app >= 0)
    {
        if(left)
            move_window(drag_app, mx - drag_dx, my - drag_dy);
        else
            drag_app = -1;
    }
    if(left && !prev_left)
        on_press(mx, my, id);
    prev_left = left;
}
static void put2(char *dst, int value)
{
    dst[0] = (char)('0' + (value / 10) % 10);
    dst[1] = (char)('0' + value % 10);
}
static void clock_poll(void)
{
    unsigned char h;
    unsigned char m;
    unsigned char s;
    if(clock_valid && timer_ticks - last_clock_poll < 10)
        return;
    last_clock_poll = timer_ticks;
    rtc_get_time(&h, &m, &s);
    if(!clock_valid || m != last_min)
    {
        int day = rtc_bcd_to_bin(rtc_read(0x07));
        int month = rtc_bcd_to_bin(rtc_read(0x08));
        int year = 2000 + rtc_bcd_to_bin(rtc_read(0x09));
        put2(time_text, h);
        time_text[2] = ':';
        put2(time_text + 3, m);
        time_text[5] = 0;
        put2(date_text, day);
        date_text[2] = '/';
        put2(date_text + 3, month);
        date_text[5] = '/';
        put2(date_text + 6, year / 100);
        put2(date_text + 8, year % 100);
        date_text[10] = 0;
        last_min = m;
        dirty_clock();
    }
    if(!clock_valid || s != last_sec)
    {
        last_sec = s;
        if(win_visible(SCREEN_HOME))
        {
            vga_surface_override(SCREEN_HOME);
            home_update_clock();
            vga_surface_override(-1);
        }
    }
    clock_valid = 1;
}
static void tick_apps(void)
{
    if(win_visible(SCREEN_SAVER) &&
       timer_ticks - last_saver_tick >= 20)
    {
        last_saver_tick = timer_ticks;
        vga_surface_override(SCREEN_SAVER);
        update_saver();
        draw_saver();
        vga_surface_override(-1);
    }
    if(focus == SCREEN_PONG &&
       timer_ticks - last_pong_tick >= 5)
    {
        last_pong_tick = timer_ticks;
        update_pong();
    }
}
static void sync_surfaces(void)
{
    int a;
    int blink = ((timer_ticks / BLINK_TICKS) & 1) == 0;
    if(vga_console_epoch() != seen_epoch)
    {
        seen_epoch = vga_console_epoch();
        cursor_drawn = 0;
        start_open = 0;
        power_open = 0;
        drag_app = -1;
        dirty_all();
    }
    for(a = 0; a < APP_COUNT; a++)
    {
        unsigned int v = vga_surface_version(a);
        if(v != seen_version[a])
        {
            seen_version[a] = v;
            if(win_visible(a))
            {
                int bw = win[a].maximized ? 0 : 1;
                dirty_add(win_x(a) + bw, win_y(a) + bw + TITLE_H,
                          win_w(a) - 2 * bw,
                          win_h(a) - 2 * bw - TITLE_H);
            }
        }
    }
    if(blink != blink_on)
    {
        blink_on = blink;
        if(focus >= 0 && win_visible(focus))
        {
            int row;
            int col;
            if(vga_surface_cursor(focus, &row, &col))
            {
                int bw = win[focus].maximized ? 0 : 1;
                int cw = win_w(focus) - 2 * bw;
                int ch = win_h(focus) - 2 * bw - TITLE_H;
                int gx = win_x(focus) + bw + (cw - CLIENT_W) / 2;
                int gy = win_y(focus) + bw + TITLE_H + (ch - CLIENT_H) / 2;
                dirty_add(gx + col * 8, gy + row * 16, 8, 16);
            }
        }
    }
}
void desktop_init(void)
{
    int i;
    if(!gfx_ready())
        return;
    scr_w = gfx_width();
    scr_h = gfx_height();
    work_h = scr_h - TASKBAR_H;
    mouse_set_bounds(scr_w - 1, scr_h - 1);
    mouse_set_position(scr_w / 2, scr_h / 2);
    vga_desktop_enter();
    seen_epoch = vga_console_epoch();
    for(i = 0; i < APP_COUNT; i++)
    {
        win[i].open = 0;
        win[i].minimized = 0;
        win[i].maximized = 0;
        win[i].x = 0;
        win[i].y = 0;
        win[i].rx = 0;
        win[i].ry = 0;
        zorder[i] = i;
        seen_version[i] = 0;
    }
    focus = -1;
    start_open = 0;
    power_open = 0;
    selected_icon = -1;
    hover = ID_NONE;
    drag_app = -1;
    prev_left = 0;
    last_click_id = -1;
    cursor_drawn = 0;
    clock_valid = 0;
    dirty_valid = 0;
    blink_on = 1;
    screen_seen = current_screen;
    launch(SCREEN_HOME);
    dirty_all();
    desktop_ready = 1;
}
void desktop_step(void)
{
    mouse_state_t m;
    int painted;
    if(!desktop_ready)
        return;
    handle_keys();
    m = mouse_get_state();
    handle_mouse(m.x, m.y, m.left);
    sync_focus();
    clock_poll();
    tick_apps();
    sync_surfaces();
    painted = render();
    update_cursor(m.x, m.y, painted);
}
void desktop_run(void)
{
    desktop_init();
    if(!desktop_ready)
        return;
    for(;;)
    {
        desktop_step();
        __asm__ volatile("hlt");
    }
}
