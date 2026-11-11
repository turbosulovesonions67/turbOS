#include "apps/editor.h"
#include "apps/app.h"
#include "gui/gfx.h"
#include "kernel/state.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "apps/terminal.h"

static void editor_refresh_cursor(void);

static int gui_rows;
static int gui_cols;

static int view_rows(void)
{
    return gui_rows > 0 ? gui_rows : 23;
}

static int view_cols(void)
{
    return gui_cols > 0 ? gui_cols : 80;
}

static void editor_ensure_visible(void)
{
    if(editor_row <= 0)
        editor_scroll_y = 0;
    else if(editor_row < editor_scroll_y)
        editor_scroll_y = editor_row;
    else if(editor_row >= editor_scroll_y + view_rows())
        editor_scroll_y = editor_row - view_rows() + 1;

    if(editor_col <= 0)
        editor_scroll_x = 0;
    else if(editor_col < editor_scroll_x)
        editor_scroll_x = editor_col;
    else if(editor_col >= editor_scroll_x + view_cols())
        editor_scroll_x = editor_col - view_cols() + 1;

    if(editor_scroll_y < 0)
        editor_scroll_y = 0;

    if(editor_scroll_x < 0)
        editor_scroll_x = 0;
}

static void draw_editor_line(int line)
{
    int screen_row;
    int c;

    if(line < editor_scroll_y)
        return;

    screen_row = line - editor_scroll_y + 2;

    if(screen_row < 2 || screen_row >= 25)
        return;

    for(c = 0; c < 80; c++)
    {
        int col = c + editor_scroll_x;

        if(col >= MAX_COLS)
            break;

        vga_put(
            editor_lines[line][col],
            screen_row,
            c,
            0x07);
    }
}

void draw_editor(void)
{
    int r;

    vga_clear(0x07);

    vga_print("turbOS Editor", 0, 0, 0x0F);
    vga_print("Ctrl+S Save", 0, 65, 0x0E);

    for(r = 0; r < 23; r++)
    {
        int line = r + editor_scroll_y;

        if(line >= MAX_LINES)
            break;

        draw_editor_line(line);
    }

    editor_refresh_cursor();
}

static void editor_refresh_cursor(void)
{
    int sy = editor_row - editor_scroll_y + 2;
    int sx = editor_col - editor_scroll_x;

    if(sy >= 2 && sy < 25 && sx >= 0 && sx < 80)
        vga_cursor(sy, sx);
    else
        vga_cursor_hide();
}

void editor_save(void)
{
    int r;
    int c;
    int k = 0;
    int last = -1;

    if(!editor_file)
        return;

    for(r = 0; r < MAX_LINES; r++)
    {
        int end = MAX_COLS;

        while(end > 0 &&
              editor_lines[r][end - 1] == ' ')
            end--;

        if(end > 0)
            last = r;
    }

    for(r = 0; r <= last; r++)
    {
        int end = MAX_COLS;

        while(end > 0 &&
              editor_lines[r][end - 1] == ' ')
            end--;

        for(c = 0; c < end; c++)
        {
            if(k >= VFS_NODE_DATA - 1)
                break;

            editor_file->data[k++] =
                editor_lines[r][c];
        }

        if(r < last &&
           k < VFS_NODE_DATA - 1)
            editor_file->data[k++] = '\n';

        if(k >= VFS_NODE_DATA - 1)
            break;
    }

    editor_file->data[k] = 0;
    editor_file->size = k;

    if(vfs_write(editor_file))
        terminal_print("File saved.");
    else
        terminal_print("Save failed.");
}

void editor_open_file(vfs_node_t *file)
{
    unsigned int i;
    int r;
    int c;

    editor_file = file;

    editor_row = 0;
    editor_col = 0;
    editor_scroll_y = 0;
    editor_scroll_x = 0;

    for(r = 0; r < MAX_LINES; r++)
        for(c = 0; c < MAX_COLS; c++)
            editor_lines[r][c] = ' ';

    i = 0;
    r = 0;
    c = 0;

    while(i < file->size &&
          i < VFS_NODE_DATA - 1 &&
          r < MAX_LINES)
    {
        if(file->data[i] == '\n')
        {
            r++;
            c = 0;
        }
        else if(file->data[i] != '\r')
        {
            if(c < MAX_COLS)
                editor_lines[r][c++] =
                    file->data[i];
        }

        i++;
    }

    current_screen = SCREEN_EDITOR;

    draw_editor();
}

void handle_editor_key(int key)
{
    if(key == KEY_CTRL)
    {
        ctrl_pressed = 1;
        return;
    }

    if(ctrl_pressed)
    {
        if(key == 's' || key == 'S')
            editor_save();

        ctrl_pressed = 0;
        return;
    }

    if(key == KEY_UP)
    {
        if(editor_row > 0)
        {
            editor_row--;

            if(editor_col > MAX_COLS - 1)
                editor_col = MAX_COLS - 1;
        }

        editor_ensure_visible();
        draw_editor();
        return;
    }

    if(key == KEY_DOWN)
    {
        if(editor_row < MAX_LINES - 1)
            editor_row++;

        editor_ensure_visible();
        draw_editor();
        return;
    }

    if(key == KEY_LEFT)
    {
        if(editor_col > 0)
        {
            editor_col--;
        }
        else if(editor_row > 0)
        {
            editor_row--;
            editor_col = MAX_COLS - 1;

            while(editor_col > 0 &&
                  editor_lines[editor_row][editor_col] == ' ')
                editor_col--;
        }

        editor_ensure_visible();
        draw_editor();
        return;
    }

    if(key == KEY_RIGHT)
    {
        if(editor_col < MAX_COLS - 1)
        {
            editor_col++;
        }
        else if(editor_row < MAX_LINES - 1)
        {
            editor_row++;
            editor_col = 0;
        }

        editor_ensure_visible();
        draw_editor();
        return;
    }

    if(key == KEY_BACKSPACE)
    {
        if(editor_col > 0)
        {
            editor_col--;
            editor_lines[editor_row][editor_col] = ' ';
        }
        else if(editor_row > 0)
        {
            int previous = editor_row - 1;
            int end = MAX_COLS - 1;

            while(end > 0 &&
                  editor_lines[previous][end] == ' ')
                end--;

            if(editor_lines[previous][end] != ' ')
                end++;

            for(int c = 0; c < MAX_COLS; c++)
            {
                if(c < MAX_COLS - end)
                    editor_lines[previous][end + c] =
                        editor_lines[editor_row][c];
                else
                    editor_lines[previous][end + c] = ' ';
            }

            for(int r = editor_row;
                r < MAX_LINES - 1;
                r++)
            {
                for(int c = 0; c < MAX_COLS; c++)
                    editor_lines[r][c] =
                        editor_lines[r + 1][c];
            }

            for(int c = 0; c < MAX_COLS; c++)
                editor_lines[MAX_LINES - 1][c] = ' ';

            editor_row = previous;
            editor_col = end;

            if(editor_col >= MAX_COLS)
                editor_col = MAX_COLS - 1;
        }

        editor_ensure_visible();
        draw_editor();
        return;
    }

    if(key == KEY_ENTER)
    {
        if(editor_row < MAX_LINES - 1)
        {
            for(int r = MAX_LINES - 1; r > editor_row + 1; r--)
            {
                for(int c = 0; c < MAX_COLS; c++)
                    editor_lines[r][c] = editor_lines[r - 1][c];
            }

            for(int c = 0; c < MAX_COLS; c++)
                editor_lines[editor_row + 1][c] = ' ';

            for(int c = editor_col; c < MAX_COLS; c++)
                editor_lines[editor_row + 1][c - editor_col] =
                    editor_lines[editor_row][c];

            for(int c = editor_col; c < MAX_COLS; c++)
                editor_lines[editor_row][c] = ' ';

            editor_row++;
            editor_col = 0;
        }

        editor_ensure_visible();
        draw_editor();
        return;
    }

    if(key >= 32 && key <= 126)
    {
        editor_lines[editor_row][editor_col] =
            (char)key;

        if(editor_col < MAX_COLS - 1)
        {
            editor_col++;
        }
        else if(editor_row < MAX_LINES - 1)
        {
            editor_col = 0;
            editor_row++;
        }

        editor_ensure_visible();
        draw_editor();
    }
}

#define ED_BG GFX_RGB(255, 255, 255)
#define ED_BAR GFX_RGB(240, 240, 240)
#define ED_LINE GFX_RGB(210, 210, 210)
#define ED_TEXT GFX_RGB(20, 20, 20)
#define ED_DIM GFX_RGB(110, 110, 110)
#define ED_SEL GFX_RGB(0, 120, 215)
#define ED_CUR GFX_RGB(238, 244, 252)
#define ED_BTN GFX_RGB(225, 225, 225)
#define ED_TOOL_H 38
#define ED_FOOT_H 26
#define ED_ROW_H 18
#define ED_GUTTER 44
#define ED_PAD 6
#define ED_SCROLL_W 14
#define ED_BTN_W 80
#define ED_BTN_H 26

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

static void append_text(char *dst, const char *src)
{
    int i = 0;

    while(dst[i])
        i++;

    while(*src && i < 63)
        dst[i++] = *src++;

    dst[i] = 0;
}

static int in_box(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && my >= y && mx < x + w && my < y + h;
}

static void editor_draw(int x, int y, int w, int h)
{
    char num[12];
    char status[64];
    const char *name = editor_file ? editor_file->name : "Untitled";
    int top = y + ED_TOOL_H;
    int track = h - ED_TOOL_H - ED_FOOT_H;
    int tx = x + ED_GUTTER + ED_PAD;
    int rows = track / ED_ROW_H;
    int cols = (w - ED_GUTTER - ED_PAD - ED_SCROLL_W) / 8;
    int r;
    int c;

    gui_rows = rows < 1 ? 1 : rows;
    gui_cols = cols < 1 ? 1 : cols;

    gfx_fill_rect(x, y, w, h, ED_BG);
    gfx_fill_rect(x, y, w, ED_TOOL_H, ED_BAR);
    gfx_hline(x, top - 1, w, ED_LINE);
    gfx_fill_rect(x + 8, y + 6, ED_BTN_W, ED_BTN_H, ED_BTN);
    gfx_rect_outline(x + 8, y + 6, ED_BTN_W, ED_BTN_H, ED_LINE);
    gfx_draw_text(x + 8 + (ED_BTN_W - gfx_text_width("Save")) / 2,
                  y + 11, "Save", ED_TEXT);
    gfx_draw_text(x + ED_BTN_W + 22, y + 11, name, ED_DIM);

    gfx_fill_rect(x, top, ED_GUTTER, track, ED_BAR);
    gfx_vline(x + ED_GUTTER, top, track, ED_LINE);

    for(r = 0; r < gui_rows; r++)
    {
        int line = editor_scroll_y + r;
        int ry = top + r * ED_ROW_H;

        if(line >= MAX_LINES)
            break;

        if(line == editor_row)
            gfx_fill_rect(x + ED_GUTTER + 1, ry, w - ED_GUTTER - 1 - ED_SCROLL_W,
                          ED_ROW_H, ED_CUR);

        number_text(line + 1, num);
        gfx_draw_text(x + ED_GUTTER - ED_PAD - gfx_text_width(num), ry + 1,
                      num, line == editor_row ? ED_TEXT : ED_DIM);

        for(c = 0; c < gui_cols; c++)
        {
            int col = editor_scroll_x + c;

            if(col >= MAX_COLS)
                break;

            if(editor_lines[line][col] != ' ')
                gfx_draw_char(tx + c * 8, ry + 1, editor_lines[line][col], ED_TEXT);
        }
    }

    r = editor_row - editor_scroll_y;
    c = editor_col - editor_scroll_x;

    if(r >= 0 && r < gui_rows && c >= 0 && c < gui_cols)
        gfx_fill_rect(tx + c * 8, top + r * ED_ROW_H + 1, 2, 16, ED_SEL);

    gfx_fill_rect(x + w - ED_SCROLL_W, top, ED_SCROLL_W, track, ED_BAR);

    {
        int thumb = track * gui_rows / MAX_LINES;
        int pos;

        if(thumb < 16)
            thumb = 16;

        pos = (track - thumb) * editor_scroll_y / (MAX_LINES - gui_rows);

        gfx_fill_rect(x + w - ED_SCROLL_W + 2, top + pos,
                      ED_SCROLL_W - 4, thumb, ED_DIM);
    }

    gfx_fill_rect(x, y + h - ED_FOOT_H, w, ED_FOOT_H, ED_BAR);
    gfx_hline(x, y + h - ED_FOOT_H, w, ED_LINE);

    status[0] = 0;
    number_text(editor_row + 1, num);
    append_text(status, "Ln ");
    append_text(status, num);
    number_text(editor_col + 1, num);
    append_text(status, ", Col ");
    append_text(status, num);
    gfx_draw_text(x + 10, y + h - ED_FOOT_H + 5, status, ED_DIM);
    gfx_draw_text(x + w - 10 - gfx_text_width("Ctrl+S Save"),
                  y + h - ED_FOOT_H + 5, "Ctrl+S Save", ED_DIM);
}

static void editor_mouse(int x, int y, int w, int h, int mx, int my)
{
    int top = y + ED_TOOL_H;
    int track = h - ED_TOOL_H - ED_FOOT_H;
    int tx = x + ED_GUTTER + ED_PAD;
    int max_scroll = MAX_LINES - view_rows();

    if(in_box(mx, my, x + 8, y + 6, ED_BTN_W, ED_BTN_H))
    {
        editor_save();
    }
    else if(in_box(mx, my, x + w - ED_SCROLL_W, top, ED_SCROLL_W, track))
    {
        if(my < top + track / 2)
            editor_scroll_y -= view_rows();
        else
            editor_scroll_y += view_rows();

        if(editor_scroll_y > max_scroll)
            editor_scroll_y = max_scroll;

        if(editor_scroll_y < 0)
            editor_scroll_y = 0;
    }
    else if(in_box(mx, my, x + ED_GUTTER, top, w - ED_GUTTER - ED_SCROLL_W, track))
    {
        int row = editor_scroll_y + (my - top) / ED_ROW_H;
        int col = editor_scroll_x + (mx - tx + 4) / 8;

        if(col < 0)
            col = 0;

        editor_row = row < MAX_LINES ? row : MAX_LINES - 1;
        editor_col = col < MAX_COLS ? col : MAX_COLS - 1;
    }
}

static void editor_icon(int x, int y, int s)
{
        app_icon_box(x, y, s, 6, 3, 20, 26, APP_WHITE);
        app_icon_box(x, y, s, 6, 3, 20, 1, GFX_RGB(200, 200, 200));
        app_icon_box(x, y, s, 10, 8, 8, 2, GFX_RGB(0, 120, 215));
        app_icon_box(x, y, s, 10, 13, 12, 2, GFX_RGB(150, 160, 170));
        app_icon_box(x, y, s, 10, 18, 12, 2, GFX_RGB(150, 160, 170));
        app_icon_box(x, y, s, 10, 23, 9, 2, GFX_RGB(150, 160, 170));
}

const app_t editor_app =
{
    SCREEN_EDITOR, "Text Editor", "Editor",
    editor_draw, editor_mouse,
    handle_editor_key, editor_icon,
    0, 0, APP_TICK_VISIBLE,
    0, draw_editor
};
