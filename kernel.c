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

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);
void outw(unsigned short port, unsigned short val);

#define SCREEN_HOME   0
#define SCREEN_SAVER  1
#define SCREEN_EDITOR 2
#define SCREEN_PONG   3

int current_screen = SCREEN_HOME;
int ctrl_pressed = 0;
int shift_pressed = 0;

int saver_counter = 0;
unsigned int pong_tick = 0;
unsigned int cursor_tick = 0;
int cursor_visible = 1;

int saver_x = 20;
int saver_y = 10;
int saver_dx = 1;
int saver_dy = 1;

#define MAX_LINES 25
#define MAX_COLS 80

char editor_lines[MAX_LINES][MAX_COLS];
int editor_row = 0;
int editor_col = 0;


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


void draw_editor()
{
    clear_screen(0x07);
    print("turbOS Editor",0,0,0x0F);

    for(int r=0;r<MAX_LINES;r++)
        for(int c=0;c<MAX_COLS;c++)
            put_char(editor_lines[r][c],r+2,c,0x0F);

    if(cursor_visible)
        put_char('_', editor_row+2, editor_col, 0x0F);
}

void handle_editor_key(unsigned char sc)
{
    if(ctrl_pressed) return;
    if(sc & 0x80) return;

    char c = scancode_table[sc];
    if(!c) return;

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


void draw_home()
{
    clear_screen(0x1F);

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
    print("Ctrl+9 Reboot",11,0,0x0F);
    print("Ctrl+0 Shutdown",12,0,0x0F);
}


void update_pong()
{
    for(int r=1;r<25;r++)
        for(int c=0;c<80;c++)
            put_char(' ',r,c,0x00);

    print("PONG",0,0,0x0F);

    char s[2]={ '0'+score,0 };
    print("Score:",0,60,0x0F);
    print(s,0,67,0x0F);

    put_char('#', paddle_y, 2, 0x0F);
    put_char('#', paddle_y+1, 2, 0x0F);
    put_char('#', paddle_y+2, 2, 0x0F);

    put_char('O', ball_y, ball_x, 0x0F);

    ball_x += ball_dx;
    ball_y += ball_dy;

    if(ball_y <= 1 || ball_y >= 24) ball_dy = -ball_dy;

    if(ball_x <= 3)
    {
        if(ball_y >= paddle_y && ball_y <= paddle_y+2)
        {
            ball_dx = 1;
            score++;
        }
        else
        {
            ball_x = 70;
            ball_y = 12;
            ball_dx = -1;
            score = 0;
        }
    }

    if(ball_x >= 79)
        ball_dx = -1;
}

void kernel_main()
{
    gdt_init();

    idt_init();
  
    /* __asm__ volatile("sti"); */
    
    pic_remap();
    
    pit_init(100);
    
    draw_home();

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

            if(sc == 29) ctrl_pressed = 1;
            if(sc == 157) ctrl_pressed = 0;

            if(sc == 42 || sc == 54) shift_pressed = 1;
            if(sc == 0xAA || sc == 0xB6) shift_pressed = 0;

            if(ctrl_pressed && sc == 2) { current_screen = SCREEN_HOME; draw_home(); }
            if(ctrl_pressed && sc == 3) { current_screen = SCREEN_SAVER; draw_saver(); }
            if(ctrl_pressed && sc == 4) { current_screen = SCREEN_EDITOR; draw_editor(); }
            if(ctrl_pressed && sc == 5) current_screen = SCREEN_PONG;

            if(ctrl_pressed && sc == 10) reboot();
            if(ctrl_pressed && sc == 11) shutdown();

            if(current_screen == SCREEN_EDITOR)
                handle_editor_key(sc);

            if(current_screen == SCREEN_PONG)
            {
                if(sc == 0x11 && paddle_y > 1) paddle_y--;
                if(sc == 0x1F && paddle_y < 22) paddle_y++;
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
