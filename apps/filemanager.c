#include "apps/filemanager.h"
#include "apps/app.h"
#include "kernel/state.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "apps/editor.h"
#include "gui/gfx.h"
#include "drivers/pit.h"

static int gui_rows;

static void ensure_visible(void)
{
    if(fm_index < fm_scroll)
        fm_scroll = fm_index;

    if(gui_rows > 0)
    {
        if(fm_index >= fm_scroll + gui_rows)
            fm_scroll = fm_index - gui_rows + 1;
    }
    else if(fm_index >= fm_scroll + 22)
        fm_scroll = fm_index - 21;

    if(fm_scroll < 0)
        fm_scroll = 0;
}

static void print_path(void)
{
    char parts[16][32];
    int count = 0;
    int col = 0;
    vfs_node_t *node = fm_dir;

    if(!node)
        return;

    vga_print("Path: ", 1, 0, 0x0E);
    col = 6;

    if(node == vfs_get_root())
    {
        vga_print("/", 1, col, 0x0F);
        return;
    }

    while(node &&
          node != vfs_get_root() &&
          count < 16)
    {
        int i = 0;

        while(node->name[i] && i < 31)
        {
            parts[count][i] = node->name[i];
            i++;
        }

        parts[count][i] = 0;
        count++;
        node = node->parent;
    }

    vga_print("/", 1, col, 0x0F);
    col++;

    for(int i = count - 1; i >= 0; i--)
    {
        int j = 0;

        vga_print(parts[i], 1, col, 0x0F);

        while(parts[i][j])
        {
            col++;
            j++;
        }

        if(i)
        {
            vga_print("/", 1, col, 0x0F);
            col++;
        }
    }
}

static void print_number(unsigned int value, int row, int col)
{
    char text[16];
    int pos = 0;
    int started = 0;
    unsigned int div = 1000000000;

    while(div > 0)
    {
        unsigned int digit = value / div;
        value %= div;
        div /= 10;

        if(digit || started || div == 0)
        {
            text[pos++] = '0' + digit;
            started = 1;
        }
    }

    text[pos] = 0;

    vga_print(text, row, col, 0x0F);
}

void draw_file_manager(void)
{
    vfs_node_t *node;
    int i = 0;
    int visible = 20;

    vga_clear(0x00);

    vga_print(
        "turbOS File Manager",
        0,
        0,
        0x0F);

    if(fm_creating)
    {
        vga_print(
            fm_new_dir ?
            "Directory name:" :
            "Filename:",
            1,
            0,
            0x0E);

        vga_print(
            fm_new_name,
            1,
            fm_new_dir ? 16 : 10,
            0x0F);

        vga_cursor(
            1,
            (fm_new_dir ? 16 : 10) +
            fm_new_name_pos);

        return;
    }

    print_path();

    if(filesystem.mounted)
    {
        unsigned int total_bytes;
        unsigned int used_bytes;
        unsigned int free_bytes;

        vga_print(
            "Disk:",
            2,
            0,
            0x0E);

        if(fat32_get_space(
                &filesystem,
                &total_bytes,
                &used_bytes,
                &free_bytes))
        {
            unsigned int total_mb =
                total_bytes / (1024 * 1024);

            unsigned int used_mb =
                used_bytes / (1024 * 1024);

            unsigned int free_mb =
                free_bytes / (1024 * 1024);

            vga_print(
                "Used",
                2,
                7,
                0x07);

            print_number(
                used_mb,
                2,
                13);

            vga_print(
                "MB",
                2,
                18,
                0x07);

            vga_print(
                "Free",
                2,
                24,
                0x07);

            print_number(
                free_mb,
                2,
                30);

            vga_print(
                "MB",
                2,
                35,
                0x07);

            vga_print(
                "Total",
                2,
                41,
                0x07);

            print_number(
                total_mb,
                2,
                48);

            vga_print(
                "MB",
                2,
                53,
                0x07);
        }
    }

    if(!fm_dir)
        return;

    node = vfs_get_child(
        fm_dir,
        fm_scroll);

    while(node && i < visible)
    {
        int row = 4 + i;

        if(fm_scroll + i == fm_index)
            vga_print(">", row, 0, 0x0F);
        else
            vga_print(" ", row, 0, 0x07);

        if(node->is_dir)
        {
            vga_print(
                "[DIR]  ",
                row,
                2,
                0x0E);

            vga_print(
                node->name,
                row,
                9,
                0x07);
        }
        else
        {
            vga_print(
                "[FILE] ",
                row,
                2,
                0x0A);

            vga_print(
                node->name,
                row,
                9,
                0x07);
        }

        node = node->next;
        i++;
    }

    vga_print(
        "N File  M Directory  D Delete  Enter Open  Backspace Up",
        24,
        0,
        0x0E);

    vga_cursor_hide();
}

void handle_file_manager_key(int key)
{
    int count;

    if(key == KEY_CTRL)
    {
        ctrl_pressed = 1;
        return;
    }

    if(fm_creating)
    {
        if(key == KEY_ESC)
        {
            fm_creating = 0;
            fm_new_dir = 0;
            fm_new_name_pos = 0;
            fm_new_name[0] = 0;
        }
        else if(key == KEY_ENTER)
        {
            if(fm_new_name_pos > 0 &&
               !vfs_find(fm_dir, fm_new_name))
            {
                vfs_node_t *node;

                if(fm_new_dir)
                    node = vfs_create_dir(
                        fm_dir,
                        fm_new_name);
                else
                    node = vfs_create_file(
                        fm_dir,
                        fm_new_name);

                if(node)
                {
                    fm_creating = 0;
                    fm_new_dir = 0;
                    fm_new_name_pos = 0;
                    fm_new_name[0] = 0;

                    fm_index =
                        vfs_count_children(fm_dir) - 1;

                    if(fm_index < 0)
                        fm_index = 0;

                    ensure_visible();
                }
            }
        }
        else if(key == KEY_BACKSPACE)
        {
            if(fm_new_name_pos > 0)
            {
                fm_new_name_pos--;
                fm_new_name[fm_new_name_pos] = 0;
            }
        }
        else if(key >= 32 && key <= 126)
        {
            if(fm_new_name_pos < 31)
            {
                fm_new_name[
                    fm_new_name_pos++] =
                    (char)key;

                fm_new_name[
                    fm_new_name_pos] = 0;
            }
        }

        draw_file_manager();
        return;
    }

    if(ctrl_pressed)
        return;

    count =
        vfs_count_children(fm_dir);

    if(key == KEY_UP)
    {
        if(fm_index > 0)
            fm_index--;

        ensure_visible();
    }
    else if(key == KEY_DOWN)
    {
        if(fm_index < count - 1)
            fm_index++;

        ensure_visible();
    }
    else if(key == 'n' || key == 'N')
    {
        fm_creating = 1;
        fm_new_dir = 0;
        fm_new_name_pos = 0;
        fm_new_name[0] = 0;
    }
    else if(key == 'm' || key == 'M')
    {
        fm_creating = 1;
        fm_new_dir = 1;
        fm_new_name_pos = 0;
        fm_new_name[0] = 0;
    }
    else if(key == 'd' || key == 'D')
    {
        vfs_node_t *selected =
            vfs_get_child(
                fm_dir,
                fm_index);

        if(selected &&
           vfs_delete(selected))
        {
            count =
                vfs_count_children(fm_dir);

            if(count == 0)
                fm_index = 0;
            else if(fm_index >= count)
                fm_index = count - 1;

            ensure_visible();
        }
    }
    else if(key == KEY_ENTER)
    {
        vfs_node_t *selected =
            vfs_get_child(
                fm_dir,
                fm_index);

        if(selected)
        {
            if(selected->is_dir)
            {
                fm_dir = selected;
                fm_index = 0;
                fm_scroll = 0;
            }
            else
                editor_open_file(selected);
        }
    }
    else if(key == KEY_BACKSPACE)
    {
        if(fm_dir &&
           fm_dir->parent)
        {
            fm_dir = fm_dir->parent;
            fm_index = 0;
            fm_scroll = 0;
        }
    }

    if(current_screen == SCREEN_FILEMANAGER)
        draw_file_manager();
}

#define FM_BG GFX_RGB(255, 255, 255)
#define FM_BAR GFX_RGB(240, 240, 240)
#define FM_LINE GFX_RGB(210, 210, 210)
#define FM_TEXT GFX_RGB(20, 20, 20)
#define FM_DIM GFX_RGB(110, 110, 110)
#define FM_SEL GFX_RGB(0, 120, 215)
#define FM_BTN GFX_RGB(225, 225, 225)
#define FM_TOOL_H 38
#define FM_PATH_H 26
#define FM_ROW_H 24
#define FM_FOOT_H 34
#define FM_SCROLL_W 14
#define FM_BTN_W 96
#define FM_BTN_H 26

static unsigned int last_row_tick;
static int last_row = -1;

static void number_text(unsigned int value, char *out)
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

static void size_text(unsigned int bytes, char *out)
{
    out[0] = 0;

    if(bytes < 1024)
    {
        char num[12];

        number_text(bytes, num);
        append_text(out, num);
        append_text(out, " B");
    }
    else
    {
        char num[12];

        number_text(bytes / 1024, num);
        append_text(out, num);
        append_text(out, " KB");
    }
}

static void gui_button(int x, int y, int w, const char *label, int enabled)
{
    int tx = x + (w - gfx_text_width(label)) / 2;

    gfx_fill_rect(x, y, w, FM_BTN_H, FM_BTN);
    gfx_rect_outline(x, y, w, FM_BTN_H, FM_LINE);
    gfx_draw_text(tx, y + 5, label, enabled ? FM_TEXT : FM_DIM);
}

static void gui_icon(int x, int y, int is_dir)
{
    if(is_dir)
    {
        gfx_fill_rect(x, y + 2, 7, 3, GFX_RGB(230, 170, 30));
        gfx_fill_rect(x, y + 4, 16, 11, GFX_RGB(255, 200, 60));
    }
    else
    {
        gfx_fill_rect(x + 2, y, 12, 16, FM_BG);
        gfx_rect_outline(x + 2, y, 12, 16, FM_DIM);
        gfx_hline(x + 5, y + 5, 6, FM_DIM);
        gfx_hline(x + 5, y + 8, 6, FM_DIM);
        gfx_hline(x + 5, y + 11, 6, FM_DIM);
    }
}

static void gui_path(char *out)
{
    vfs_node_t *chain[16];
    int count = 0;
    vfs_node_t *node = fm_dir;
    int i;

    out[0] = 0;
    append_text(out, "/");

    while(node && node != vfs_get_root() && count < 16)
    {
        chain[count++] = node;
        node = node->parent;
    }

    for(i = count - 1; i >= 0; i--)
    {
        append_text(out, chain[i]->name);

        if(i)
            append_text(out, "/");
    }
}

static int list_top(int y)
{
    return y + FM_TOOL_H + FM_PATH_H;
}

static int list_rows(int h)
{
    int rows = (h - FM_TOOL_H - FM_PATH_H - FM_FOOT_H) / FM_ROW_H;

    return rows < 1 ? 1 : rows;
}

static void fm_draw(int x, int y, int w, int h)
{
    char line[64];
    int rows = list_rows(h);
    int count = vfs_count_children(fm_dir);
    int top = list_top(y);
    int i;
    int bx = x + 8;
    vfs_node_t *node;

    gui_rows = rows;
    ensure_visible();

    gfx_fill_rect(x, y, w, h, FM_BG);
    gfx_fill_rect(x, y, w, FM_TOOL_H, FM_BAR);
    gfx_hline(x, y + FM_TOOL_H - 1, w, FM_LINE);
    gui_button(bx, y + 6, FM_BTN_W, "Up", fm_dir && fm_dir->parent);
    gui_button(bx + (FM_BTN_W + 6), y + 6, FM_BTN_W, "New file", 1);
    gui_button(bx + (FM_BTN_W + 6) * 2, y + 6, FM_BTN_W, "New folder", 1);
    gui_button(bx + (FM_BTN_W + 6) * 3, y + 6, FM_BTN_W, "Delete", count > 0);

    gui_path(line);
    gfx_fill_rect(x, y + FM_TOOL_H, w, FM_PATH_H, FM_BG);
    gfx_draw_text(x + 10, y + FM_TOOL_H + 5, line, FM_TEXT);
    gfx_hline(x, top - 1, w, FM_LINE);

    node = vfs_get_child(fm_dir, fm_scroll);

    for(i = 0; i < rows && node; i++)
    {
        int ry = top + i * FM_ROW_H;
        int selected = fm_scroll + i == fm_index;
        uint32_t fg = selected ? GFX_RGB(255, 255, 255) : FM_TEXT;
        int sx = x + w - FM_SCROLL_W - 100;

        if(selected)
            gfx_fill_rect(x, ry, w - FM_SCROLL_W, FM_ROW_H, FM_SEL);

        gui_icon(x + 10, ry + 4, node->is_dir);
        gfx_draw_text(x + 34, ry + 4, node->name, fg);

        if(node->is_dir)
            gfx_draw_text(sx, ry + 4, "Folder", selected ? fg : FM_DIM);
        else
        {
            size_text(node->size, line);
            gfx_draw_text(sx, ry + 4, line, selected ? fg : FM_DIM);
        }

        node = node->next;
    }

    if(count == 0)
        gfx_draw_text(x + 34, top + 4, "This folder is empty", FM_DIM);

    gfx_fill_rect(x + w - FM_SCROLL_W, top, FM_SCROLL_W, rows * FM_ROW_H, FM_BAR);

    if(count > rows)
    {
        int track = rows * FM_ROW_H;
        int thumb = track * rows / count;
        int pos = (track - thumb) * fm_scroll / (count - rows);

        if(thumb < 16)
            thumb = 16;

        gfx_fill_rect(x + w - FM_SCROLL_W + 2, top + pos, FM_SCROLL_W - 4, thumb, FM_DIM);
    }

    gfx_fill_rect(x, y + h - FM_FOOT_H, w, FM_FOOT_H, FM_BAR);
    gfx_hline(x, y + h - FM_FOOT_H, w, FM_LINE);

    if(fm_creating)
    {
        int fy = y + h - FM_FOOT_H + 4;
        int field_w = w - 20 - 2 * (FM_BTN_W / 2 + 6) - 70;

        gfx_draw_text(x + 10, fy + 5, fm_new_dir ? "Folder:" : "File:", FM_TEXT);
        gfx_fill_rect(x + 80, fy, field_w, FM_BTN_H, FM_BG);
        gfx_rect_outline(x + 80, fy, field_w, FM_BTN_H, FM_SEL);
        gfx_draw_text(x + 86, fy + 5, fm_new_name, FM_TEXT);
        gfx_fill_rect(x + 86 + gfx_text_width(fm_new_name), fy + 5, 2, 16, FM_TEXT);
        gui_button(x + w - 2 * (FM_BTN_W / 2 + 6) - 6, fy, FM_BTN_W / 2, "Create", 1);
        gui_button(x + w - (FM_BTN_W / 2 + 6), fy, FM_BTN_W / 2, "Cancel", 1);
    }
    else if(filesystem.mounted)
    {
        unsigned int total;
        unsigned int used;
        unsigned int avail;

        if(fat32_get_space(&filesystem, &total, &used, &avail))
        {
            char num[12];

            line[0] = 0;
            number_text(used / (1024 * 1024), num);
            append_text(line, "Used ");
            append_text(line, num);
            append_text(line, " MB   Free ");
            number_text(avail / (1024 * 1024), num);
            append_text(line, num);
            append_text(line, " MB   Total ");
            number_text(total / (1024 * 1024), num);
            append_text(line, num);
            append_text(line, " MB");
            gfx_draw_text(x + 10, y + h - FM_FOOT_H + 9, line, FM_DIM);
        }
    }
}

static int in_box(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && my >= y && mx < x + w && my < y + h;
}

static void fm_mouse(int x, int y, int w, int h, int mx, int my)
{
    int rows = list_rows(h);
    int count = vfs_count_children(fm_dir);
    int top = list_top(y);
    int bx = x + 8;

    if(fm_creating)
    {
        int fy = y + h - FM_FOOT_H + 4;

        if(in_box(mx, my, x + w - 2 * (FM_BTN_W / 2 + 6) - 6, fy, FM_BTN_W / 2, FM_BTN_H))
            handle_file_manager_key(KEY_ENTER);
        else if(in_box(mx, my, x + w - (FM_BTN_W / 2 + 6), fy, FM_BTN_W / 2, FM_BTN_H))
            handle_file_manager_key(KEY_ESC);

        return;
    }

    if(in_box(mx, my, bx, y + 6, FM_BTN_W, FM_BTN_H))
        handle_file_manager_key(KEY_BACKSPACE);
    else if(in_box(mx, my, bx + FM_BTN_W + 6, y + 6, FM_BTN_W, FM_BTN_H))
        handle_file_manager_key('n');
    else if(in_box(mx, my, bx + (FM_BTN_W + 6) * 2, y + 6, FM_BTN_W, FM_BTN_H))
        handle_file_manager_key('m');
    else if(in_box(mx, my, bx + (FM_BTN_W + 6) * 3, y + 6, FM_BTN_W, FM_BTN_H))
        handle_file_manager_key('d');
    else if(in_box(mx, my, x + w - FM_SCROLL_W, top, FM_SCROLL_W, rows * FM_ROW_H))
    {
        int middle = top + rows * FM_ROW_H / 2;

        if(my < middle && fm_scroll > 0)
            fm_scroll = fm_scroll > rows ? fm_scroll - rows : 0;
        else if(my >= middle && fm_scroll + rows < count)
            fm_scroll += rows;
    }
    else if(in_box(mx, my, x, top, w - FM_SCROLL_W, rows * FM_ROW_H))
    {
        int row = fm_scroll + (my - top) / FM_ROW_H;

        if(row < count)
        {
            int again = row == last_row && timer_ticks - last_row_tick <= 50;

            fm_index = row;
            last_row = row;
            last_row_tick = timer_ticks;

            if(again)
            {
                last_row = -1;
                handle_file_manager_key(KEY_ENTER);
            }
        }
    }
}

static void file_manager_icon(int x, int y, int s)
{
        app_icon_box(x, y, s, 2, 6, 13, 6, GFX_RGB(227, 168, 30));
        app_icon_box(x, y, s, 2, 10, 28, 17, GFX_RGB(255, 211, 90));
        app_icon_box(x, y, s, 2, 15, 28, 12, GFX_RGB(255, 224, 138));
}

const app_t file_manager_app =
{
    SCREEN_FILEMANAGER, "File Manager", "Files",
    fm_draw, fm_mouse,
    handle_file_manager_key, file_manager_icon,
    0, 0, APP_TICK_VISIBLE,
    0, draw_file_manager
};
