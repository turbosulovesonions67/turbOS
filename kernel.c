#include "gdt/gdt.h"
#include "interrupts/idt.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/ps2.h"
#include "drivers/ata.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "fs/vfs.h"
#include "fs/fat32.h"

#define SCREEN_HOME 0
#define SCREEN_SAVER 1
#define SCREEN_EDITOR 2
#define SCREEN_PONG 3
#define SCREEN_TERMINAL 4
#define SCREEN_FILEMANAGER 5

#define MAX_LINES 512
#define MAX_COLS 80

static const char *username = "liveuser";
static const char *hostname = "turbOS";

static int current_screen = SCREEN_HOME;

static int ctrl_pressed = 0;

static unsigned char home_color = 0x1F;

static char terminal_input[80];
static int terminal_pos;

static char terminal_output[20][80];
static int output_lines;

static int editor_row;
static int editor_col;
static int editor_scroll_y;
static int editor_scroll_x;

static char editor_lines[MAX_LINES][MAX_COLS];

static vfs_node_t *editor_file;
static vfs_node_t *fm_dir;
static int fm_index;
static fat32_fs_t filesystem;

static int fm_creating;
static char fm_new_name[32];
static int fm_new_name_pos;

static int saver_x = 20;
static int saver_y = 10;
static int saver_dx = 1;
static int saver_dy = 1;

static int ball_x = 70;
static int ball_y = 12;
static int ball_dx = -1;
static int ball_dy = 1;
static int paddle_y = 10;
static int score;

static unsigned int last_second;
static unsigned int last_saver_tick;
static unsigned int last_pong_tick;

static void halt(void)
{
    for(;;)
        __asm__ volatile("hlt");
}

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

static unsigned char inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile("inb %1,%0" : "=a"(value) : "Nd"(port));

    return value;
}

static void outw(unsigned short port, unsigned short value)
{
    __asm__ volatile("outw %0,%1" : : "a"(value), "Nd"(port));
}

static int strlen(const char *s)
{
    int i = 0;

    while(s[i])
        i++;

    return i;
}

static int strcmp(const char *a, const char *b)
{
    while(*a && *b)
    {
        if(*a != *b)
            return 1;

        a++;
        b++;
    }

    return *a != *b;
}

static unsigned char bcd_to_bin(unsigned char value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

static unsigned char cmos_read(unsigned char reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static void clear(unsigned char color)
{
    vga_clear(color);
}

static void print(
    const char *s,
    int row,
    int col,
    unsigned char color)
{
    vga_print(s, row, col, color);
}

static void draw_clock(void)
{
    unsigned char h;
    unsigned char m;
    unsigned char s;
    char time[9];

    h = bcd_to_bin(cmos_read(0x04));
    m = bcd_to_bin(cmos_read(0x02));
    s = bcd_to_bin(cmos_read(0x00));

    time[0] = '0' + h / 10;
    time[1] = '0' + h % 10;
    time[2] = ':';
    time[3] = '0' + m / 10;
    time[4] = '0' + m % 10;
    time[5] = ':';
    time[6] = '0' + s / 10;
    time[7] = '0' + s % 10;
    time[8] = 0;

    print("Time:", 4, 0, 0x0F);
    print(time, 4, 6, 0x0F);
}

static void draw_home(void)
{
    unsigned char h;

    clear(home_color);

    print("Welcome to turbOS!", 0, 0, 0x0F);

    h = bcd_to_bin(cmos_read(0x04));

    if(h < 12)
        print("Good Morning!", 2, 0, 0x0F);
    else if(h < 17)
        print("Good Afternoon!", 2, 0, 0x0F);
    else if(h < 21)
        print("Good Evening!", 2, 0, 0x0F);
    else
        print("Good Night!", 2, 0, 0x0F);

    draw_clock();

    print("Ctrl+1 Home", 7, 0, 0x0F);
    print("Ctrl+2 Saver", 8, 0, 0x0F);
    print("Ctrl+3 Editor", 9, 0, 0x0F);
    print("Ctrl+4 Pong", 10, 0, 0x0F);
    print("Ctrl+5 Terminal", 11, 0, 0x0F);
    print("Ctrl+6 File Manager", 12, 0, 0x0F);
    print("Ctrl+9 Reboot", 13, 0, 0x0F);
    print("Ctrl+0 Shutdown", 14, 0, 0x0F);

    vga_cursor_hide();
}

static void draw_saver(void)
{
    clear(0x01);

    print(
        "Ctrl+1 Home Ctrl+3 Editor Ctrl+4 Pong",
        0,
        0,
        0x0F);

    print("turbOS", saver_y, saver_x, 0x0F);

    vga_cursor_hide();
}

static void update_saver(void)
{
    saver_x += saver_dx;
    saver_y += saver_dy;

    if(saver_x <= 0 || saver_x >= 74)
        saver_dx = -saver_dx;

    if(saver_y <= 1 || saver_y >= 24)
        saver_dy = -saver_dy;
}

static void terminal_print(const char *s);
static void editor_refresh_cursor(void);

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
            0x07
        );
    }
}

static void draw_editor(void)
{
    clear(0x07);

    print("turbOS Editor", 0, 0, 0x0F);
    print("Ctrl+S Save", 0, 65, 0x0E);

    for(int r = 0; r < 23; r++)
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

static void editor_save(void)
{
    if(!editor_file)
        return;

    int k = 0;

    for(int r = 0; r < MAX_LINES; r++)
    {
        for(int c = 0; c < MAX_COLS; c++)
        {
            if(k < VFS_NODE_DATA - 1)
                editor_file->data[k++] = editor_lines[r][c];
        }
    }

    editor_file->data[k] = 0;
    editor_file->size = k;

    if(vfs_write(editor_file))
        terminal_print("File saved.");
    else
        terminal_print("Save failed.");
}

static void editor_open_file(vfs_node_t *file)
{
    editor_file = file;

    editor_row = 0;
    editor_col = 0;
    editor_scroll_y = 0;
    editor_scroll_x = 0;

    for(int r = 0; r < MAX_LINES; r++)
        for(int c = 0; c < MAX_COLS; c++)
            editor_lines[r][c] = ' ';

    int i = 0;
    int r = 0;
    int c = 0;

    while(i < VFS_NODE_DATA - 1 &&
          i < MAX_LINES * MAX_COLS &&
          file->data[i])
    {
        editor_lines[r][c] = file->data[i++];

        c++;

        if(c >= MAX_COLS)
        {
            c = 0;
            r++;

            if(r >= MAX_LINES)
                break;
        }
    }

    current_screen = SCREEN_EDITOR;

    draw_editor();
}

static void handle_editor_key(int key)
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
            editor_row--;

        editor_refresh_cursor();
        return;
    }

    if(key == KEY_DOWN)
    {
        if(editor_row < MAX_LINES - 1)
            editor_row++;

        editor_refresh_cursor();
        return;
    }

    if(key == KEY_LEFT)
    {
        if(editor_col > 0)
            editor_col--;

        editor_refresh_cursor();
        return;
    }

    if(key == KEY_RIGHT)
    {
        if(editor_col < MAX_COLS - 1)
            editor_col++;

        editor_refresh_cursor();
        return;
    }

    if(key == KEY_BACKSPACE)
    {
        if(editor_col > 0)
        {
            editor_col--;
            editor_lines[editor_row][editor_col] = ' ';
            draw_editor_line(editor_row);
        }

        editor_refresh_cursor();
        return;
    }

    if(key == KEY_ENTER)
    {
        if(editor_row < MAX_LINES - 1)
        {
            editor_row++;
            editor_col = 0;
        }

        editor_refresh_cursor();
        return;
    }

    if(key >= 32 && key <= 126)
    {
        editor_lines[editor_row][editor_col] = (char)key;

        vga_put(
            (char)key,
            editor_row - editor_scroll_y + 2,
            editor_col - editor_scroll_x,
            0x07
        );

        if(editor_col < MAX_COLS - 1)
            editor_col++;
        else if(editor_row < MAX_LINES - 1)
        {
            editor_col = 0;
            editor_row++;
        }

        editor_refresh_cursor();
    }
}

static void terminal_print(const char *s)
{
    if(output_lines >= 20)
        output_lines = 0;

    int i = 0;

    while(s[i] && i < 79)
    {
        terminal_output[output_lines][i] = s[i];
        i++;
    }

    terminal_output[output_lines][i] = 0;
    output_lines++;
}

static void draw_terminal(void)
{
    clear(0x00);

    print("turbOS Terminal", 0, 0, 0x0F);
    print("Type 'help' for commands", 1, 0, 0x07);

    for(int i = 0; i < output_lines && i < 20; i++)
        print(terminal_output[i], 3 + i, 0, 0x07);

    int row = 3 + output_lines;

    print(username, row, 0, 0x0F);
    print("@", row, strlen(username), 0x0F);
    print(hostname, row, strlen(username) + 1, 0x0F);

    int col =
        strlen(username) +
        1 +
        strlen(hostname);

    print(">", row, col, 0x0F);
    print(" ", row, col + 1, 0x0F);
    print(terminal_input, row, col + 2, 0x0F);

    vga_cursor_hide();
}

static void execute_command(void)
{
    if(!strcmp(terminal_input, "help"))
    {
        terminal_print("help");
        terminal_print("reboot");
        terminal_print("powoff");
        terminal_print("clear");
        terminal_print("time");
        terminal_print("about");
        terminal_print("ver");
        terminal_print("fetch");
        terminal_print("exit");
    }
    else if(!strcmp(terminal_input, "ver"))
    {
        terminal_print("turbOS v0.2");
    }
    else if(!strcmp(terminal_input, "about"))
    {
        terminal_print("turbOS");
        terminal_print("Created by Turbosu Pramanik");
        terminal_print("i386 kernel");
    }
    else if(!strcmp(terminal_input, "clear"))
    {
        output_lines = 0;
    }
    else if(!strcmp(terminal_input, "time"))
    {
        terminal_print("Use the Home clock.");
    }
    else if(!strcmp(terminal_input, "fetch"))
    {
        terminal_print("ttt\\");
        terminal_print("ttt\\");
        terminal_print("tttttttt\\");
        terminal_print("ttt\\");
        terminal_print("ttt\\");
        terminal_print("  tttttt\n");
        terminal_print("turbOS v0.2");
        terminal_print("Kernel : TASK-32bit");
        terminal_print("Arch   : i386");
        terminal_print("Shell  : turbCMD!");
    }
    else if(!strcmp(terminal_input, "exit"))
    {
        current_screen = SCREEN_HOME;
        draw_home();
        return;
    }
    else if(!strcmp(terminal_input, "reboot"))
    {
        outb(0x64, 0xFE);
        halt();
    }
    else if(!strcmp(terminal_input, "powoff"))
    {
        clear(0x00);
        print("Shutting down turbOS...", 10, 25, 0x0F);

        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        outw(0x4004, 0x3400);

        halt();
    }
    else if(!strcmp(terminal_input, "bg -c"))
    {
        home_color = 0x3F;
        terminal_print("Background changed.");
    }
    else if(terminal_input[0])
    {
        terminal_print("turbCMD!: Unknown command");
    }
}

static void handle_terminal_key(int key)
{
    if(key == KEY_CTRL)
    {
        ctrl_pressed = 1;
        return;
    }

    if(ctrl_pressed)
        return;

    if(key == KEY_BACKSPACE)
    {
        if(terminal_pos > 0)
        {
            terminal_pos--;
            terminal_input[terminal_pos] = 0;
        }

        draw_terminal();
        return;
    }

    if(key == KEY_ENTER)
    {
        execute_command();

        terminal_pos = 0;
        terminal_input[0] = 0;

        if(current_screen == SCREEN_TERMINAL)
            draw_terminal();

        return;
    }

    if(key >= 32 && key <= 126)
    {
        if(terminal_pos < 79)
        {
            terminal_input[terminal_pos++] =
                (char)key;

            terminal_input[terminal_pos] = 0;
        }

        draw_terminal();
    }
}

static void draw_file_manager(void)
{
    clear(0x00);

    print("turbOS File Manager", 0, 0, 0x0F);

    if(fm_creating)
    {
        print("Filename:", 1, 0, 0x0E);
        print(fm_new_name, 1, 10, 0x0F);
        vga_cursor(1, 10 + fm_new_name_pos);
        return;
    }

    print("N: New File   D: Delete   Enter: Open   Backspace: Up", 1, 0, 0x0E);

    if(!fm_dir)
        return;

    vfs_node_t *node = fm_dir->child;
    int i = 0;

    while(node && i < 22)
    {
        if(i == fm_index)
            print(">", i + 2, 0, 0x0F);
        else
            print(" ", i + 2, 0, 0x07);

        if(node->is_dir)
            print("[DIR] ", i + 2, 2, 0x0E);
        else
            print("[FILE] ", i + 2, 2, 0x0A);

        print(node->name, i + 2, 9, 0x07);

        node = node->next;
        i++;
    }

    vga_cursor_hide();
}

static void handle_file_manager_key(int key)
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
                vfs_node_t *new_file =
                    vfs_create_file(fm_dir, fm_new_name);

                if(new_file)
                {
                    fm_creating = 0;
                    fm_new_name_pos = 0;
                    fm_new_name[0] = 0;
                    fm_index = vfs_count_children(fm_dir) - 1;
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
                fm_new_name[fm_new_name_pos++] = (char)key;
                fm_new_name[fm_new_name_pos] = 0;
            }
        }

        draw_file_manager();
        return;
    }

    if(ctrl_pressed)
        return;

    count = vfs_count_children(fm_dir);

    if(key == KEY_UP)
    {
        if(fm_index > 0)
            fm_index--;
    }
    else if(key == KEY_DOWN)
    {
        if(fm_index < count - 1)
            fm_index++;
    }
    else if(key == 'n' || key == 'N')
    {
        fm_creating = 1;
        fm_new_name_pos = 0;
        fm_new_name[0] = 0;
    }
    else if(key == 'd' || key == 'D')
    {
        vfs_node_t *selected =
            vfs_get_child(fm_dir, fm_index);

        if(selected && !selected->is_dir)
        {
            if(vfs_delete(selected))
            {
                count = vfs_count_children(fm_dir);

                if(count == 0)
                    fm_index = 0;
                else if(fm_index >= count)
                    fm_index = count - 1;
            }
        }
    }
    else if(key == KEY_ENTER)
    {
        vfs_node_t *selected =
            vfs_get_child(fm_dir, fm_index);

        if(selected)
        {
            if(selected->is_dir)
            {
                fm_dir = selected;
                fm_index = 0;
            }
            else
                editor_open_file(selected);
        }
    }
    else if(key == KEY_BACKSPACE)
    {
        if(fm_dir && fm_dir->parent)
        {
            fm_dir = fm_dir->parent;
            fm_index = 0;
        }
    }

    if(current_screen == SCREEN_FILEMANAGER)
        draw_file_manager();
}

static void draw_pong(void)
{
    clear(0x00);

    print("PONG", 0, 0, 0x0F);

    char score_text[12];
    int value = score;
    int pos = 0;

    if(value == 0)
    {
        score_text[pos++] = '0';
    }
    else
    {
        char reversed[12];
        int count = 0;

        while(value > 0 && count < 11)
        {
            reversed[count++] = '0' + (value % 10);
            value /= 10;
        }

        while(count > 0)
            score_text[pos++] = reversed[--count];
    }

    score_text[pos] = '\0';

    print("Score:", 0, 70 - pos, 0x0F);
    print(score_text, 0, 76 - pos, 0x0F);

    vga_put('#', paddle_y, 2, 0x0F);
    vga_put('#', paddle_y + 1, 2, 0x0F);
    vga_put('#', paddle_y + 2, 2, 0x0F);

    vga_put('O', ball_y, ball_x, 0x0F);

    vga_cursor_hide();
}

static void update_pong(void)
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

static void process_global_key(int key)
{
    if(key == KEY_CTRL)
    {
        ctrl_pressed = 1;
        return;
    }

    if(!ctrl_pressed)
        return;

    if(current_screen == SCREEN_EDITOR &&
       (key == 's' || key == 'S'))
    {
        editor_save();
        return;
    }

    if(key == '1')
    {
        current_screen = SCREEN_HOME;
        draw_home();
    }
    else if(key == '2')
    {
        current_screen = SCREEN_SAVER;
        draw_saver();
    }
    else if(key == '3')
    {
        current_screen = SCREEN_EDITOR;
        draw_editor();
    }
    else if(key == '4')
    {
        current_screen = SCREEN_PONG;
        draw_pong();
    }
    else if(key == '5')
    {
        current_screen = SCREEN_TERMINAL;
        draw_terminal();
    }
    else if(key == '6')
    {
        current_screen = SCREEN_FILEMANAGER;
        draw_file_manager();
    }
    else if(key == '9')
    {
        outb(0x64, 0xFE);
        halt();
    }
    else if(key == '0')
    {
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        halt();
    }
}

void kernel_main(void)
{
    gdt_init();

    idt_init();

    pic_remap();

    pit_init(100);

    keyboard_init();

    ps2_init();
    mouse_init();

    if(fat32_mount(&filesystem, 0))
    {
        vfs_init(&filesystem);
        terminal_print("FAT32 mounted.");
    }
    else
    {
        filesystem.mounted = 0;
        vfs_init(&filesystem);
    }

    fm_dir = vfs_get_root();
    fm_index = 0;

    draw_home();

    __asm__ volatile("sti");

    last_second = bcd_to_bin(cmos_read(0x00));

    for(;;)
    {
        int key;

        while((key = keyboard_getkey()) != KEY_NONE)
        {
            if(key == KEY_CTRL)
            {
                ctrl_pressed = 1;
                continue;
            }

            if(ctrl_pressed)
            {
                process_global_key(key);
                ctrl_pressed = 0;
                continue;
            }

            if(current_screen == SCREEN_EDITOR)
                handle_editor_key(key);
            else if(current_screen == SCREEN_TERMINAL)
                handle_terminal_key(key);
            else if(current_screen == SCREEN_FILEMANAGER)
                handle_file_manager_key(key);
            else if(current_screen == SCREEN_PONG)
            {
                if(key == 'w' && paddle_y > 1)
                    paddle_y--;

                if(key == 's' && paddle_y < 22)
                    paddle_y++;
            }
        }

        if(current_screen == SCREEN_HOME)
        {
            unsigned int sec =
                bcd_to_bin(cmos_read(0x00));

            if(sec != last_second)
            {
                last_second = sec;
                draw_clock();
            }
        }

        if(current_screen == SCREEN_SAVER)
        {
            if(timer_ticks - last_saver_tick >= 20)
            {
                last_saver_tick = timer_ticks;
                update_saver();
                draw_saver();
            }
        }

        if(current_screen == SCREEN_PONG)
        {
            if(timer_ticks - last_pong_tick >= 5)
            {
                last_pong_tick = timer_ticks;
                update_pong();
            }
        }

        __asm__ volatile("hlt");
    }
}
