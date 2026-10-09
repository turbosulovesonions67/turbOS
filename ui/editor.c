#include "ui/editor.h"
#include "kernel/state.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "ui/terminal.h"

static void editor_refresh_cursor(void);

static void editor_ensure_visible(void)
{
    if(editor_row <= 0)
        editor_scroll_y = 0;
    else if(editor_row < editor_scroll_y)
        editor_scroll_y = editor_row;
    else if(editor_row >= editor_scroll_y + 23)
        editor_scroll_y = editor_row - 22;

    if(editor_col <= 0)
        editor_scroll_x = 0;
    else if(editor_col < editor_scroll_x)
        editor_scroll_x = editor_col;
    else if(editor_col >= editor_scroll_x + 80)
        editor_scroll_x = editor_col - 79;

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
