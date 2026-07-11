/* turbOS C kernel by Turbosu Pramanik */
/* A Monolithic Preemtipve kernel for my proto/pilot OS project-turbOS */
/* License: GNU GPL 3.0 */
/* This OS kernel is subject to WEEKLY UPDATES!! */
/* This project is dedicated towards the legendary Terrence Andrew Davis, (Terry!) who wrote an OS single handedly - TempleOS. Fly High, Captain! (RIP 2018) */

/* The Code Goes here: */

#include "gdt/gdt.h"
#include "interrupts/idt.h"
#include "drivers/ata.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "fs/vfs.h"

const char* username = "liveuser";
const char* hostname = "turbOS";

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);
void outw(unsigned short port, unsigned short val);

#define SCREEN_HOME   0
#define SCREEN_SAVER  1
#define SCREEN_EDITOR 2
#define SCREEN_PONG   3
#define SCREEN_TERMINAL 4
#define SCREEN_FILEMANAGER 5
#define MAX_FILES 32

int fm_index = 0;
vfs_node_t* fm_dir;

void draw_home();
void draw_editor();
void draw_terminal();
void handle_editor_key(unsigned char sc);
void handle_terminal_key(unsigned char sc);
void editor_open_file(vfs_node_t* file);
void editor_save();

void reboot();
void shutdown();

int current_screen = SCREEN_HOME;
int ctrl_pressed = 0;
int shift_pressed = 0;

char cmd_buffer[80];
char arg_buffer[80];

char terminal_input[80];
int terminal_pos = 0;

unsigned char home_color = 0x1F;

char terminal_output[20][80];
int output_lines = 0;

int saver_counter = 0;
unsigned int pong_tick = 0;
unsigned int cursor_tick = 0;
int cursor_visible = 1;

int saver_x = 20;
int saver_y = 10;
int saver_dx = 1;
int saver_dy = 1;

#define MAX_LINES 512
#define MAX_COLS 80

char editor_lines[MAX_LINES][MAX_COLS];

int editor_row = 0;
int editor_col = 0;

int editor_scroll_y = 0;
int editor_scroll_x = 0;

vfs_node_t* editor_file = 0;

int ball_x = 70;
int ball_y = 12;
int ball_dx = -1;
int ball_dy = 1;

int paddle_y = 10;
int score = 0;

char scancode_table[128] =
{
    0,27,
    '1','2','3','4','5','6','7','8','9','0',
    '-','=',
    '\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p',
    '[',']',
    '\n',
    ' ',
    'a','s','d','f','g','h','j','k','l',
    ';','\'',
    '`',
    0,
    '\\',
    'z','x','c','v','b','n','m',
    ',', '.', '/'
};

char shift_map(char c)
{
    switch(c)
    {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';

        case '-': return '_';
        case '=': return '+';

        case '[': return '{';
        case ']': return '}';

        case ';': return ':';
        case '\'': return '"';

        case ',': return '<';
        case '.': return '>';
        case '/': return '?';

        default: return c;
    }
}

void clear_screen(unsigned char color)
{
    volatile unsigned char* v = (unsigned char*)0xB8000;

    for(int i = 0; i < 80 * 25; i++)
    {
        v[i * 2] = ' ';
        v[i * 2 + 1] = color;
    }
}

void put_char(char c, int r, int col, unsigned char color)
{
    if(r < 0 || r >= 25 || col < 0 || col >= 80) return;

    volatile unsigned char* v = (unsigned char*)0xB8000;
    int i = (r * 80 + col) * 2;

    v[i] = c;
    v[i + 1] = color;
}

void print(const char* s, int r, int c, unsigned char color)
{
    for(int i = 0; s[i]; i++)
        put_char(s[i], r, c + i, color);
}

int strcmp(const char* a, const char* b)
{
    while(*a && *b)
    {
        if(*a != *b)
            return 1;

        a++;
        b++;
    }

    return (*a != *b);
}

int strlen(const char* s)
{
    int i = 0;
    while (s[i]) i++;
    return i;
}

void cmd_help();
void cmd_ver();
void cmd_clear();
void cmd_time();
void cmd_reboot();
void cmd_powoff();
void cmd_about();
void cmd_exit();

char* skip_spaces(char* s)
{
    while(*s == ' ') s++;
    return s;
}

typedef void (*cmd_func)(void);

typedef struct {
    const char* name;
    cmd_func func;
} command_t;

void terminal_print(const char* s)
{
    if(output_lines >= 20)
        output_lines = 0;

    int i;

    for(i = 0; s[i] && i < 79; i++)
        terminal_output[output_lines][i] = s[i];

    terminal_output[output_lines][i] = 0;

    output_lines++;
}

unsigned char cmos_read(unsigned char reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

unsigned char bcd_to_bin(unsigned char b)
{
    return ((b >> 4) * 10) + (b & 0xF);
}

void draw_clock()
{
    while(cmos_read(0x0A) & 0x80);

    unsigned char s = bcd_to_bin(cmos_read(0x00));
    unsigned char m = bcd_to_bin(cmos_read(0x02));
    unsigned char h = bcd_to_bin(cmos_read(0x04));

    char t[9];

    t[0]='0'+h/10; t[1]='0'+h%10;
    t[2]=':';
    t[3]='0'+m/10; t[4]='0'+m%10;
    t[5]=':';
    t[6]='0'+s/10; t[7]='0'+s%10;
    t[8]=0;

    print("Time:",4,0,0x0F);
    print("        ",4,6,0x0F);
    print(t,4,6,0x0F);
}

void halt(){ for(;;) __asm__("hlt"); }

void shutdown()
{
    clear_screen(0x00);
    print("Shutting down turbOS...",10,25,0x0F);

    outw(0x604,0x2000);
    outw(0xB004,0x2000);
    outw(0x4004,0x3400);

    halt();
}

void reboot()
{
    clear_screen(0x00);
    print("Rebooting turbOS...",10,25,0x0F);

    outb(0x64,0xFE);
    halt();
}


void update_saver()
{
    saver_x += saver_dx;
    saver_y += saver_dy;

    if(saver_x <= 0 || saver_x >= 74) saver_dx = -saver_dx;
    if(saver_y <= 1 || saver_y >= 24) saver_dy = -saver_dy;
}

void draw_saver()
{
    clear_screen(0x01);
    print("Ctrl+1 Home Ctrl+3 Editor Ctrl+4 Pong",0,0,0x0F);
    print("turbOS",saver_y,saver_x,0x0F);
}


void draw_editor_cursor()
{
    int sy = editor_row - editor_scroll_y + 2;
    int sx = editor_col - editor_scroll_x;

    if(sy < 2 || sy >= 25 || sx < 0 || sx >= 80)
        return;

    if(cursor_visible)
        put_char('_', sy, sx, 0x0F);
    else
        put_char(editor_lines[editor_row][editor_col], sy, sx, 0x0F);
}

void draw_editor()
{
    clear_screen(0x07);
    print("turbOS Editor",0,0,0x0F);

    for(int r = 0; r < 23; r++)
    {
        int line = r + editor_scroll_y;

        if(line >= MAX_LINES)
            break;

        for(int c = 0; c < 80; c++)
        {
            int col = c + editor_scroll_x;

            if(col >= MAX_COLS)
                break;

            put_char(editor_lines[line][col], r + 2, c, 0x0F);
        }
    }

    if(cursor_visible)
    {
        int sy = editor_row - editor_scroll_y + 2;
        int sx = editor_col - editor_scroll_x;

        if(sy >= 2 && sy < 25 && sx >= 0 && sx < 80)
            put_char('_', sy, sx, 0x0F);
    }
}

void handle_editor_key(unsigned char sc)
{
    if(ctrl_pressed && sc == 0x1F) // S
    {
        editor_save();
        return;
    }

    if(ctrl_pressed) return;
    if(sc & 0x80) return;

    char c = scancode_table[sc];
    
    if(sc == 0x48)
    {
        if(editor_row > 0)
            editor_row--;

        draw_editor();
        return;
    }

    if(sc == 0x50)
    {
        if(editor_row < MAX_LINES - 1)
            editor_row++;

        draw_editor();
        return;
    }

    if(sc == 0x4B)
    {
        if(editor_col > 0)
            editor_col--;

        draw_editor();
        return;
    }

    if(sc == 0x4D)
    {
        if(editor_col < MAX_COLS - 1)
            editor_col++;

        draw_editor();
        return;
    }

    if(sc == 0x39)   
        c = ' ';

    if(shift_pressed)
    {
        if(c >= 'a' && c <= 'z') c -= 32;
        else c = shift_map(c);
    }

    if(c == '\n')
    {
        if(shift_pressed)
        {
            editor_row++;
            editor_col = 0;

            if(editor_row >= MAX_LINES)
                editor_row = MAX_LINES - 1;
        }
        else
        {
            editor_lines[editor_row][editor_col] = ' ';

            editor_col++;

            if(editor_col >= MAX_COLS)
            {
                editor_col = 0;
                editor_row++;

                if(editor_row >= MAX_LINES)
                    editor_row = MAX_LINES - 1;
            }
        }

        return;
    }

    if(c == '\b')
    {
        if(editor_col > 0)
        {
            editor_col--;
        }
        else if(editor_row > 0)
        {
            editor_row--;
            editor_col = MAX_COLS - 1;
        }

        editor_lines[editor_row][editor_col] = ' ';
        return;
    }

    editor_lines[editor_row][editor_col] = c;

    editor_col++;
    if(editor_col >= MAX_COLS)
    {
        editor_col = 0;
        editor_row++;
        if(editor_row >= MAX_LINES) editor_row = MAX_LINES - 1;
    }
}

void editor_save()
{
    if(!editor_file) return;

    int k = 0;

    for(int r = 0; r < MAX_LINES; r++)
    {
        for(int c = 0; c < MAX_COLS; c++)
        {
            editor_file->data[k++] = editor_lines[r][c];
        }
    }

    editor_file->data[k] = 0;
}

void editor_open_file(vfs_node_t* file)
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
    int r = 0, c = 0;

    while(file->data[i] && i < MAX_LINES * MAX_COLS)
    {
        editor_lines[r][c] = file->data[i];

        c++;
        if(c >= MAX_COLS)
        {
            c = 0;
            r++;
        }

        i++;
    }

    current_screen = SCREEN_EDITOR;
    draw_editor();
}

void draw_file_manager()
{
    clear_screen(0x00);

    print("turbOS File Manager", 0, 0, 0x0F);

    vfs_node_t* cur = fm_dir->child;

    int i = 0;

    while(cur && i < 25)
    {
        if(i == fm_index)
            print(" > ", i+2, 0, 0x0F);
        else
            print("   ", i+2, 0, 0x07);

        if(cur->is_dir)
    {
        print("[DIR] ", i+2, 3, 0x0E);
        print(cur->name, i+2, 9, 0x07);
    }
    else
    {
        print("[FILE]", i+2, 3, 0x0A);
        print(cur->name, i+2, 9, 0x07);
    }

        cur = cur->next;
        i++;
    }
}


void handle_file_manager_key(unsigned char sc)
{
    if(sc & 0x80) return;

    int count = vfs_count_children(fm_dir);

    if(sc == 0x48)
    {
        if(fm_index > 0)
            fm_index--;
    }

    if(sc == 0x50)
    {
        if(fm_index < count - 1)
            fm_index++;
    }
    
    if(sc == 0x1C)
    {
        vfs_node_t* sel = vfs_get_child(fm_dir, fm_index);

        if(!sel) return;

        if(sel->is_dir)
        {
            fm_dir = sel;
            fm_index = 0;
        }
        else
        {
            editor_open_file(sel);
        }
    }
    if(sc == 0x0E)
    {
        if(fm_dir && fm_dir->parent)
        {
            fm_dir = fm_dir->parent;
            fm_index = 0;
        }
    }

    draw_file_manager();
}
void cmd_help()
{
    terminal_print("turbcmd commands:");
    terminal_print("help, ver, clear, time");
    terminal_print("reboot, powoff, about, exit");
}

void cmd_ver()
{
    terminal_print("turbOS v0.1.42");
}

void cmd_clear()
{
    output_lines = 0;
}

void cmd_time()
{
    terminal_print("Use home screen clock");
}

void cmd_reboot()
{
    reboot();
}

void cmd_powoff()
{
    shutdown();
}

void cmd_about()
{
    terminal_print("turbOS v0.1.42");
    terminal_print("Created by Turbosu Pramanik");
    terminal_print("Designed for older computers");
}

void cmd_exit()
{
    current_screen = SCREEN_HOME;
    draw_home();
}

void draw_terminal()
{
    clear_screen(0x00);

    print("turbOS Terminal v0.1.5", 0, 0, 0x0F);
    print("Type 'help' for commands", 1, 0, 0x07);

    for(int i = 0; i < output_lines; i++)
        print(terminal_output[i], 3 + i, 0, 0x07);

    int row = 4 + output_lines;
    int col = 0;

    print(username, row, col, 0x0F);
    col += strlen(username);

    print("@", row, col, 0x0F);
    col += 1;

    print(hostname, row, col, 0x0F);
    col += strlen(hostname);

    print(">", row, col, 0x0F);
    col += 2;

    print(" ", row, col, 0x0F);
    col += 1;

    print(terminal_input, row, col, 0x0F);
}

void cmd_fetch()
{
    terminal_print("        ttt     \\\\");
    terminal_print("        tttttt   \\\\");
    terminal_print("        ttt       \\\\");
    terminal_print("         ttttttt   \\\\");

    terminal_print("");
    terminal_print("turbOS v0.1.5");
    terminal_print("Kernel : turboX-32bit");
    terminal_print("Arch   : i386");
    terminal_print("Shell  : turbCMD!");
    terminal_print("User   : liveuser");
    terminal_print("Host   : turbOS");
}

void execute_command()
{
    if(!strcmp(terminal_input, "help"))
    {
        terminal_print("help");
        terminal_print("reboot");
        terminal_print("powoff");
        terminal_print("bg -c");
        terminal_print("about");
        terminal_print("time");
        terminal_print("clear");
        terminal_print("ver");
        terminal_print("exit");
        terminal_print("fetch");
    }
    else if(!strcmp(terminal_input, "ver"))
    {
        terminal_print("turbOS v0.1.5");
    }
    else if(!strcmp(terminal_input, "about"))
    {
        terminal_print("turbOS v0.1.5");
        terminal_print("Created by Turbosu Pramanik");
        terminal_print("Designed for older computers");
    }
    else if(!strcmp(terminal_input, "clear"))
    {
        output_lines = 0;
    }
    else if(!strcmp(terminal_input, "time"))
    {
        terminal_print("Use Home screen clock");
    }
    else if(!strcmp(terminal_input, "exit"))
    {
        current_screen = SCREEN_HOME;
        draw_home();
        return;
    }
    else if(!strcmp(terminal_input, "reboot"))
    {
        reboot();
    }
    else if(!strcmp(terminal_input, "powoff"))
    {
        shutdown();
    }
    else if(!strcmp(terminal_input, "bg -c"))
    {
        home_color = 0x3F;
        terminal_print("Background changed to cyan");
    }
    else if(!strcmp(terminal_input, "fetch"))
    {
        cmd_fetch();
    }
    else
    {
        terminal_print("turbCMD!: Unknown command");
    }
}


void handle_terminal_key(unsigned char sc)
{
    if(sc & 0x80)
        return;

    char c = scancode_table[sc];

    if(sc == 0x39)   // SPACE key
        c = ' ';

    if(c == '\b')
    {
        if(terminal_pos > 0)
        {
            terminal_pos--;
            terminal_input[terminal_pos] = 0;
        }

        draw_terminal();
        return;
    }
    if(c == '\n')
    {
        execute_command();

        terminal_pos = 0;
        terminal_input[0] = 0;

        draw_terminal();
        return;
    }
    if(terminal_pos < 79)
    {
        terminal_input[terminal_pos++] = c;
        terminal_input[terminal_pos] = 0;
    }

    draw_terminal();
}

void draw_home()
{
    clear_screen(home_color);

    print("Welcome to turbOS!",0,0,0x0F);

    unsigned char h = bcd_to_bin(cmos_read(0x04));

    if(h<12) print("Good Morning!",2,0,0x0F);
    else if(h<17) print("Good Afternoon!",2,0,0x0F);
    else if(h<21) print("Good Evening!",2,0,0x0F);
    else print("Good Night!",2,0,0x0F);

    draw_clock();

    print("Ctrl+1 Home",7,0,0x0F);
    print("Ctrl+2 Saver",8,0,0x0F);
    print("Ctrl+3 Editor",9,0,0x0F);
    print("Ctrl+4 Pong",10,0,0x0F);
    print("Ctrl+5 Terminal",11,0,0x0F);
    print("Ctrl+6 File Manager",12,0,0x0F);
    print("Ctrl+9 Reboot",13,0,0x0F);
    print("Ctrl+0 Shutdown",14,0,0x0F);
}


void update_pong()
{
    for(int r = 1; r < 25; r++)
        for(int c = 0; c < 80; c++)
            put_char(' ', r, c, 0x00);

    print("PONG", 0, 0, 0x0F);

    char s[10];
    int temp = score;
    int i = 0;

    if(temp == 0)
    {
        s[i++] = '0';
    }
    else
    {
        char rev[10];
        int j = 0;

        while(temp > 0)
        {
            rev[j++] = '0' + (temp % 10);
            temp /= 10;
        }

        while(j > 0)
            s[i++] = rev[--j];
    }

    s[i] = 0;

    print("Score:", 0, 60, 0x0F);
    print(s, 0, 67, 0x0F);

    put_char('#', paddle_y, 2, 0x0F);
    put_char('#', paddle_y+1, 2, 0x0F);
    put_char('#', paddle_y+2, 2, 0x0F);

    put_char('O', ball_y, ball_x, 0x0F);

    ball_x += ball_dx;
    ball_y += ball_dy;

    if(ball_y <= 1)
    {
        ball_y = 1;
        ball_dy = 1;
    }
    else if(ball_y >= 24)
    {
        ball_y = 24;
        ball_dy = -1;
    }

    if(ball_x <= 3 && ball_dx < 0)
    {
        if(ball_y >= paddle_y && ball_y <= paddle_y + 2)
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
            ball_dy = (ball_y % 2 == 0) ? 1 : -1;
            score = 0;
        }
    }

    if(ball_x >= 79)
    {
        ball_x = 79;
        ball_dx = -1;
    }
}

void kernel_main()
{
    gdt_init();

    idt_init();

    pic_remap();

    pit_init(100);

    draw_home();
    
    vfs_init();
    fm_dir = vfs_get_root();
    fm_index = 0;

    int last_sec = -1;

    while(1)
    {
        if(current_screen == SCREEN_HOME)
        {
            unsigned char s = bcd_to_bin(cmos_read(0x00));

            if(s != last_sec)
            {
                last_sec = s;
                draw_clock();
            }
        }

        if(current_screen == SCREEN_SAVER)
        {
            saver_counter++;

            if(saver_counter > 180000)
            {
                saver_counter = 0;

                update_saver();
                draw_saver();
            }
        }

        if(current_screen == SCREEN_EDITOR)
        {
            cursor_tick++;

            if(cursor_tick > 40000)
            {
                cursor_tick = 0;
                cursor_visible = !cursor_visible;

                draw_editor();
            }
        }
        if(current_screen == SCREEN_PONG)
        {
            pong_tick++;

            if(pong_tick > 120000)
            {
                pong_tick = 0;

                update_pong();
            }
        }
        
        if(inb(0x64) & 1)
        {
            unsigned char sc = inb(0x60);

            if(sc == 29)
                ctrl_pressed = 1;

            if(sc == 157)
                ctrl_pressed = 0;


            if(sc == 42 || sc == 54)
                shift_pressed = 1;

            if(sc == 0xAA || sc == 0xB6)
                shift_pressed = 0;


            if(ctrl_pressed && sc == 2)
            {
                current_screen = SCREEN_HOME;
                draw_home();
            }

            if(ctrl_pressed && sc == 3)
            {
                current_screen = SCREEN_SAVER;
                draw_saver();
            }

            if(ctrl_pressed && sc == 4)
            {
                current_screen = SCREEN_EDITOR;
                draw_editor();
            }

            if(ctrl_pressed && sc == 5)
            {
                current_screen = SCREEN_PONG;

                clear_screen(0x00);
                update_pong();
            }

            if(ctrl_pressed && sc == 6)
            {
                current_screen = SCREEN_TERMINAL;
                draw_terminal();
            }
            
            if(ctrl_pressed && sc == 7)
            {
                current_screen = SCREEN_FILEMANAGER;
                draw_file_manager();
            }

            if(ctrl_pressed && sc == 10)
                reboot();

            if(ctrl_pressed && sc == 11)
                shutdown();

            if(current_screen == SCREEN_EDITOR)
            {
                handle_editor_key(sc);
            }

            if(current_screen == SCREEN_TERMINAL)
            {
                handle_terminal_key(sc);
            }
            
            if(current_screen == SCREEN_FILEMANAGER)
            {
                handle_file_manager_key(sc);
            }

            
            if(current_screen == SCREEN_PONG)
            {
                if(sc == 0x11 && paddle_y > 1)
                    paddle_y--;

                if(sc == 0x1F && paddle_y < 22)
                    paddle_y++;
            }
        }
    }
}


void outb(unsigned short p, unsigned char v)
{ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

unsigned char inb(unsigned short p)
{
    unsigned char r;
    __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p));
    return r;
}

void outw(unsigned short p, unsigned short v)
{ 
    __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); 
    
}
