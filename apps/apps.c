#include "apps/app.h"
#include "apps/terminal.h"
#include "apps/filemanager.h"
#include "apps/editor.h"
#include "apps/pong.h"
#include "apps/saver.h"
#include "gui/gfx.h"
const app_t *const app_table[APP_COUNT] =
{
    &terminal_app,
    &file_manager_app,
    &editor_app,
    &pong_app,
    &saver_app
};
const app_t *app_by_screen(int screen)
{
    int i;
    for(i = 0; i < APP_COUNT; i++)
    {
        if(app_table[i]->screen == screen)
            return app_table[i];
    }
    return 0;
}
int app_icon_sc(int v, int s)
{
    return (v * s + 16) / 32;
}
void app_icon_box(int x, int y, int s,
                  int ax, int ay, int aw, int ah, uint32_t c)
{
    int x0 = x + app_icon_sc(ax, s);
    int y0 = y + app_icon_sc(ay, s);
    int x1 = x + app_icon_sc(ax + aw, s);
    int y1 = y + app_icon_sc(ay + ah, s);
    if(x1 <= x0)
        x1 = x0 + 1;
    if(y1 <= y0)
        y1 = y0 + 1;
    gfx_fill_rect(x0, y0, x1 - x0, y1 - y0, c);
}
