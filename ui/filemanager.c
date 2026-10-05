#include "ui/filemanager.h"
#include "kernel/state.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "ui/editor.h"

static void ensure_visible(void)
{
    if(fm_index < fm_scroll)
        fm_scroll = fm_index;

    if(fm_index >= fm_scroll + 22)
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
        if(key == KEY_ENTER)
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
