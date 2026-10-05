#include "kernel/state.h"

const char *username = "liveuser";
const char *hostname = "turbOS";

int current_screen = SCREEN_HOME;
int ctrl_pressed = 0;

unsigned char home_color = 0x1F;

char terminal_input[80];
int terminal_pos = 0;

char terminal_output[TERMINAL_HISTORY][80];
int output_lines = 0;
int terminal_scroll = 0;

int editor_row = 0;
int editor_col = 0;
int editor_scroll_y = 0;
int editor_scroll_x = 0;

char editor_lines[MAX_LINES][MAX_COLS];

vfs_node_t *editor_file = 0;
vfs_node_t *fm_dir = 0;
int fm_index = 0;
int fm_scroll = 0;
fat32_fs_t filesystem;

int fm_creating = 0;
int fm_new_dir = 0;
char fm_new_name[32];
int fm_new_name_pos = 0;

int saver_x = 20;
int saver_y = 10;
int saver_dx = 1;
int saver_dy = 1;

int ball_x = 70;
int ball_y = 12;
int ball_dx = -1;
int ball_dy = 1;
int paddle_y = 10;
int score = 0;

unsigned int last_second = 0;
unsigned int last_saver_tick = 0;
unsigned int last_pong_tick = 0;

int boot_line = 0;
