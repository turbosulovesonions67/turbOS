#ifndef TURBOS_KERNEL_STATE_H
#define TURBOS_KERNEL_STATE_H

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

#define TERMINAL_HISTORY 128
#define TERMINAL_WIDTH 80

extern const char *username;
extern const char *hostname;

extern int current_screen;
extern int ctrl_pressed;

extern unsigned char home_color;

extern char terminal_input[80];
extern int terminal_pos;

extern char terminal_output[TERMINAL_HISTORY][80];
extern int output_lines;
extern int terminal_scroll;

extern int editor_row;
extern int editor_col;
extern int editor_scroll_y;
extern int editor_scroll_x;

extern char editor_lines[MAX_LINES][MAX_COLS];

extern vfs_node_t *editor_file;
extern vfs_node_t *fm_dir;
extern int fm_index;
extern int fm_scroll;
extern fat32_fs_t filesystem;

extern int fm_creating;
extern int fm_new_dir;
extern char fm_new_name[32];
extern int fm_new_name_pos;

extern int saver_x;
extern int saver_y;
extern int saver_dx;
extern int saver_dy;

extern int ball_x;
extern int ball_y;
extern int ball_dx;
extern int ball_dy;
extern int paddle_y;
extern int score;

extern unsigned int last_second;
extern unsigned int last_saver_tick;
extern unsigned int last_pong_tick;
extern int boot_line;

#endif
