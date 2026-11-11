#include <stdarg.h>
#include "apps/terminal.h"
#include "apps/app.h"
#include "gui/gfx.h"
#include "kernel/state.h"
#include "kernel/memory.h"
#include "kernel/accounts.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "power/power.h"
#include "time/rtc.h"
#include "kernel/version.h"
#include "apps/filemanager.h"
#include "apps/editor.h"
#include "fs/vfs.h"
#include "runtime/tex.h"
#include "compiler/turbix.h"
#include "drivers/pit.h"

static int strcmp_local(const char *a, const char *b)
{
    while(*a && *a == *b)
    {
        a++;
        b++;
    }

    return (unsigned char)*a -
           (unsigned char)*b;
}

static int strlen_local(const char *s)
{
    int i = 0;

    while(s[i])
        i++;

    return i;
}

static void copy_string(char *dst, const char *src, int max)
{
    int i = 0;

    if(max <= 0)
        return;

    while(src[i] && i < max - 1)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static void clear_line(char *line)
{
    for(int i = 0; i < TERMINAL_WIDTH; i++)
        line[i] = 0;
}

static void terminal_push_line(const char *s)
{
    if(output_lines < TERMINAL_HISTORY)
    {
        copy_string(
            terminal_output[output_lines],
            s,
            TERMINAL_WIDTH);

        output_lines++;
        return;
    }

    for(int i = 1; i < TERMINAL_HISTORY; i++)
        copy_string(
            terminal_output[i - 1],
            terminal_output[i],
            TERMINAL_WIDTH);

    copy_string(
        terminal_output[TERMINAL_HISTORY - 1],
        s,
        TERMINAL_WIDTH);
}

void terminal_print(const char *s)
{
    char line[TERMINAL_WIDTH];
    int pos = 0;

    if(!s)
        return;

    clear_line(line);

    for(int i = 0;; i++)
    {
        char c = s[i];

        if(c == '\n' || c == 0)
        {
            line[pos] = 0;
            terminal_push_line(line);

            pos = 0;
            clear_line(line);

            if(c == 0)
                break;

            continue;
        }

        line[pos++] = c;

        if(pos >= TERMINAL_WIDTH - 1)
        {
            line[pos] = 0;
            terminal_push_line(line);

            pos = 0;
            clear_line(line);
        }
    }

    terminal_scroll = 0;
}

static void build_path(char *path)
{
    char parts[16][32];
    int count = 0;
    int p = 0;

    vfs_node_t *node = fm_dir;

    if(!node || node == vfs_get_root())
    {
        copy_string(
            path,
            "/",
            TERMINAL_WIDTH);

        return;
    }

    while(node &&
          node != vfs_get_root() &&
          count < 16)
    {
        copy_string(
            parts[count],
            node->name,
            32);

        count++;
        node = node->parent;
    }

    path[p++] = '/';

    for(int i = count - 1; i >= 0; i--)
    {
        for(int j = 0;
            parts[i][j] &&
            p < TERMINAL_WIDTH - 1;
            j++)
        {
            path[p++] = parts[i][j];
        }

        if(i && p < TERMINAL_WIDTH - 1)
            path[p++] = '/';
    }

    path[p] = 0;
}

static void build_prompt(char *prompt)
{
    char path[TERMINAL_WIDTH];
    int p = 0;

    build_path(path);

    for(int i = 0;
        username[i] &&
        p < TERMINAL_WIDTH - 1;
        i++)
        prompt[p++] = username[i];

    if(p < TERMINAL_WIDTH - 1)
        prompt[p++] = '@';

    for(int i = 0;
        hostname[i] &&
        p < TERMINAL_WIDTH - 1;
        i++)
        prompt[p++] = hostname[i];

    if(p < TERMINAL_WIDTH - 1)
        prompt[p++] = ':';

    for(int i = 0;
        path[i] &&
        p < TERMINAL_WIDTH - 1;
        i++)
        prompt[p++] = path[i];

    if(p < TERMINAL_WIDTH - 1)
        prompt[p++] = '$';

    prompt[p] = 0;
}

static void draw_prompt(int row)
{
    char prompt[TERMINAL_WIDTH];

    build_prompt(prompt);

    vga_print(
        prompt,
        row,
        0,
        0x0A);

    if(terminal_pos > 0)
    {
        vga_print(
            " ",
            row,
            strlen_local(prompt),
            0x07);

        vga_print(
            terminal_input,
            row,
            strlen_local(prompt) + 1,
            0x07);
    }
}

static void draw_history_line(int index, int row)
{
    char prompt[TERMINAL_WIDTH];
    int prompt_len;
    int is_prompt = 1;

    build_prompt(prompt);
    prompt_len = strlen_local(prompt);

    for(int i = 0;
        i < prompt_len &&
        i < TERMINAL_WIDTH - 1;
        i++)
    {
        if(terminal_output[index][i] != prompt[i])
        {
            is_prompt = 0;
            break;
        }
    }

    if(is_prompt &&
       strlen_local(terminal_output[index]) >= prompt_len)
    {
        char command[TERMINAL_WIDTH];
        int p = 0;

        for(int i = 0;
            i < prompt_len &&
            p < TERMINAL_WIDTH - 1;
            i++)
        {
            command[p++] =
                terminal_output[index][i];
        }

        command[p] = 0;

        vga_print(
            command,
            row,
            0,
            0x0A);

        if(terminal_output[index][prompt_len])
        {
            int q = 0;

            while(terminal_output[index]
                    [prompt_len + 1 + q] &&
                  q < TERMINAL_WIDTH - 1)
            {
                command[q] =
                    terminal_output[index]
                    [prompt_len + 1 + q];

                q++;
            }

            command[q] = 0;

            vga_print(
                command,
                row,
                prompt_len + 1,
                0x07);
        }

        return;
    }

    vga_print(
        terminal_output[index],
        row,
        0,
        0x07);
}

void draw_terminal(void)
{
    int visible = 23;
    int start =
        output_lines -
        visible -
        terminal_scroll;

    if(start < 0)
        start = 0;

    vga_clear(0x00);

    vga_print(
        "turbOS Terminal",
        0,
        0,
        0x0F);

    for(int i = 0;
        i < visible &&
        start + i < output_lines;
        i++)
    {
        draw_history_line(
            start + i,
            1 + i);
    }

    if(terminal_scroll == 0)
    {
        int prompt_row =
            1 +
            output_lines -
            start;

        if(prompt_row > 24)
            prompt_row = 24;

        draw_prompt(prompt_row);
    }

    vga_cursor_hide();
}

static vfs_node_t *resolve_child(
    vfs_node_t *dir,
    const char *name)
{
    if(!dir || !name || !name[0])
        return dir;

    if(!strcmp_local(name, "."))
        return dir;

    if(!strcmp_local(name, ".."))
    {
        if(dir->parent)
            return dir->parent;

        return dir;
    }

    return vfs_find(dir, name);
}

static vfs_node_t *resolve_path(
    vfs_node_t *base,
    const char *path)
{
    char part[32];
    int p = 0;

    vfs_node_t *current;

    if(!path || !path[0])
        return base;

    current =
        path[0] == '/' ?
        vfs_get_root() :
        base;

    for(int i = 0;; i++)
    {
        char c = path[i];

        if(c == '/' || c == 0)
        {
            if(p)
            {
                part[p] = 0;

                current =
                    resolve_child(
                        current,
                        part);

                if(!current)
                    return 0;

                if(!current->is_dir &&
                   c != 0)
                    return 0;

                p = 0;
            }

            if(c == 0)
                break;

            continue;
        }

        if(p < 31)
            part[p++] = c;
    }

    return current;
}

static void command_ls(void)
{
    vfs_node_t *node;

    if(!fm_dir)
        return;

    node = fm_dir->child;

    if(!node)
    {
        terminal_print("(empty)");
        return;
    }

    while(node)
    {
        char line[TERMINAL_WIDTH];
        int p = 0;

        const char *prefix =
            node->is_dir ?
            "[DIR]  " :
            "[FILE] ";

        for(int i = 0;
            prefix[i] &&
            p < TERMINAL_WIDTH - 1;
            i++)
            line[p++] = prefix[i];

        for(int i = 0;
            node->name[i] &&
            p < TERMINAL_WIDTH - 1;
            i++)
            line[p++] = node->name[i];

        line[p] = 0;

        terminal_print(line);

        node = node->next;
    }
}

static void command_pwd(void)
{
    char path[TERMINAL_WIDTH];

    build_path(path);
    terminal_print(path);
}

static void terminal_add_command(void)
{
    char prompt[TERMINAL_WIDTH];
    char line[TERMINAL_WIDTH];
    int p = 0;

    build_prompt(prompt);

    for(int i = 0;
        prompt[i] &&
        p < TERMINAL_WIDTH - 1;
        i++)
        line[p++] = prompt[i];

    if(p < TERMINAL_WIDTH - 1)
        line[p++] = ' ';

    for(int i = 0;
        terminal_input[i] &&
        p < TERMINAL_WIDTH - 1;
        i++)
        line[p++] = terminal_input[i];

    line[p] = 0;

    terminal_push_line(line);
}

static void make_tex_name(
    const char *source_name,
    char *output_name)
{
    int i = 0;
    int dot = -1;

    while(source_name[i] && i < 30)
    {
        if(source_name[i] == '.')
            dot = i;

        output_name[i] = source_name[i];
        i++;
    }

    if(dot >= 0)
    {
        output_name[dot++] = '.';
        output_name[dot++] = 't';
        output_name[dot++] = 'e';
        output_name[dot++] = 'x';
        output_name[dot] = 0;
    }
    else
    {
        output_name[i++] = '.';
        output_name[i++] = 't';
        output_name[i++] = 'e';
        output_name[i++] = 'x';
        output_name[i] = 0;
    }
}

static int parse_compile_argument(
    const char *arg,
    char *filename)
{
    int i = 0;

    while(arg[i] == ' ')
        i++;

    if(arg[i++] != '-')
        return 0;

    if(arg[i++] != 'c')
        return 0;

    if(arg[i] != ' ')
        return 0;

    while(arg[i] == ' ')
        i++;

    if(!arg[i])
        return 0;

    {
        int j = 0;

        while(arg[i] && j < 79)
            filename[j++] = arg[i++];

        filename[j] = 0;
    }

    return 1;
}

#define TC_CYAN "\001"
#define TC_YELLOW "\002"
#define TC_RESET "\007"
#define TC_WHITE "\010"
#define END ((const char *)0)
#define KERNEL_KIB (((unsigned int)stack_top - 0x100000U) / 1024U)

extern char stack_top[];

static const char *const fetch_logo[] =
{
    TC_WHITE "ttt        \\\\\\",
    TC_WHITE "ttt          \\\\\\",
    TC_WHITE "tttttttt      \\\\\\",
    TC_WHITE "ttt            \\\\\\",
    TC_WHITE "ttt              \\\\\\",
    TC_WHITE "  tttttt          \\\\\\"
};

static int fetch_pos;

static void text_add(char *dst, const char *src)
{
    int i = strlen_local(dst);

    while(*src && i < TERMINAL_WIDTH - 1)
        dst[i++] = *src++;

    dst[i] = 0;
}

static void text_pad(char *dst, int width)
{
    int shown = 0;
    int i = 0;

    while(dst[i])
        shown += dst[i++] >= 9;

    while(shown++ < width && i < TERMINAL_WIDTH - 1)
        dst[i++] = ' ';

    dst[i] = 0;
}

static void text_uint(char *dst, unsigned int value)
{
    char digit[2];

    if(value >= 10)
        text_uint(dst, value / 10);

    digit[0] = (char)('0' + value % 10);
    digit[1] = 0;
    text_add(dst, digit);
}

static void text_kib(char *dst, unsigned int kib)
{
    text_uint(dst, kib >= 1024 ? kib / 1024 : kib);
    text_add(dst, kib >= 1024 ? " MiB" : " KiB");
}

static void text_usage(char *dst, unsigned int used, unsigned int total)
{
    text_kib(dst, used);
    text_add(dst, " / ");
    text_kib(dst, total);
}

static const char *kib_text(unsigned int kib)
{
    static char pool[4][16];
    static int next;
    char *out = pool[next++ & 3];

    out[0] = 0;
    text_kib(out, kib);
    return out;
}

static void cols(const char *cell, ...)
{
    char line[TERMINAL_WIDTH];
    va_list args;
    int n = 0;

    line[0] = 0;
    va_start(args, cell);

    for(; cell; cell = va_arg(args, const char *))
    {
        text_add(line, cell);
        text_pad(line, ++n * 10);
    }

    va_end(args);
    terminal_print(line);
}

static void command_lsblk(void)
{
    unsigned int lba;
    unsigned int sectors;
    unsigned int total;
    unsigned int used;
    unsigned int avail;

    if(!vfs_get_storage_info(&lba, &sectors) ||
       !fat32_get_space(&filesystem, &total, &used, &avail))
    {
        terminal_print("No mounted storage volume");
        return;
    }

    cols(TC_CYAN "NAME", "TYPE", "FSTYPE", "SIZE", "USED", "FREE", "MOUNT", END);
    cols(TC_RESET "hda", lba ? "part" : "disk", "fat32", kib_text(sectors / 2U),
         kib_text(used / 1024U), kib_text(avail / 1024U), "/", END);
}

static void command_ram(void)
{
    unsigned int total = memory_total_mib * 1024U;

    if(!memory_map_available)
    {
        terminal_print("Memory map unavailable.");
        terminal_print("Check Multiboot2 bootloader support.");
        return;
    }

    cols(TC_CYAN "", "total", "kernel", "available", END);
    cols(TC_YELLOW "Mem" TC_RESET, kib_text(total), kib_text(KERNEL_KIB),
         kib_text(total - KERNEL_KIB), END);
}

static void fetch_row(const char *key, const char *value)
{
    char line[TERMINAL_WIDTH];

    line[0] = 0;

    if(fetch_pos < (int)(sizeof(fetch_logo) / sizeof(fetch_logo[0])))
        text_add(line, fetch_logo[fetch_pos]);

    fetch_pos++;
    text_pad(line, 24);
    text_add(line, TC_CYAN);

    if(key)
    {
        text_add(line, key);
        text_add(line, ": " TC_RESET);
    }

    text_add(line, value);
    terminal_print(line);
}

static void command_fetch(void)
{
    char value[TERMINAL_WIDTH];
    char rule[TERMINAL_WIDTH];
    char block[] = "\001###";
    unsigned int total;
    unsigned int used;
    unsigned int avail;
    unsigned int up = timer_ticks / 100U;
    int i;

    fetch_pos = 0;
    value[0] = 0;
    text_add(value, username);
    text_add(value, "@");
    text_add(value, hostname);
    fetch_row(0, value);

    for(i = 0; value[i]; i++)
        rule[i] = '-';

    rule[i] = 0;
    fetch_row(0, rule);
    fetch_row("OS", "turbOS v" TURBOS_VERSION " i386");
    fetch_row("Kernel", "TASK-32bit");

    value[0] = 0;
    text_uint(value, up / 3600U);
    text_add(value, "h ");
    text_uint(value, up / 60U % 60U);
    text_add(value, "m ");
    text_uint(value, up % 60U);
    text_add(value, "s");
    fetch_row("Uptime", value);
    fetch_row("Shell", "turbCMD!");

    value[0] = 0;
    text_uint(value, gfx_width());
    text_add(value, "x");
    text_uint(value, gfx_height());
    fetch_row("Resolution", value);

    value[0] = 0;

    if(memory_map_available)
        text_usage(value, KERNEL_KIB, memory_total_mib * 1024U);
    else
        text_add(value, "unavailable");

    fetch_row("Memory", value);

    value[0] = 0;

    if(fat32_get_space(&filesystem, &total, &used, &avail))
    {
        text_usage(value, used / 1024U, total / 1024U);
        text_add(value, " (fat32)");
    }
    else
        text_add(value, "none");

    fetch_row("Disk (/)", value);
    fetch_row(0, "");

    value[0] = 0;

    for(; block[0] < 7; block[0]++)
        text_add(value, block);

    fetch_row(0, value);
}

static void execute_command(void)
{
    char command[80];
    char arg[80];

    int i = 0;
    int j = 0;

    while(terminal_input[i] == ' ')
        i++;

    while(terminal_input[i] &&
          terminal_input[i] != ' ' &&
          j < 79)
    {
        command[j++] =
            terminal_input[i++];
    }

    command[j] = 0;

    while(terminal_input[i] == ' ')
        i++;

    j = 0;

    while(terminal_input[i] &&
          j < 79)
    {
        arg[j++] =
            terminal_input[i++];
    }

    arg[j] = 0;

    if(!strcmp_local(command, "echo"))
    {
        terminal_print(arg);
    }
    else if(!strcmp_local(command, "help"))
    {
        terminal_print("help       show commands");
        terminal_print("echo <txt> print text");
        terminal_print("ls         list directory");
        terminal_print("cd <dir>   change directory");
        terminal_print("pwd        show current directory");
        terminal_print("mkdir <d>  create directory");
        terminal_print("touch <f>  create file");
        terminal_print("cat <f>    print file");
        terminal_print("rm <f>     delete file");
        terminal_print("rmdir <d>  delete empty directory");
        terminal_print("fm         open file manager");
        terminal_print("open <f>   open file in editor");
        terminal_print("run <f>    execute TEX program");
        terminal_print("turbix -c <f> compile Turbix");
        terminal_print("clear      clear terminal");
        terminal_print("time       show the time");
        terminal_print("about      show system information");
        terminal_print("ver        show version");
        terminal_print("fetch      show system info");
        terminal_print("reboot     reboot");
        terminal_print("powoff     power off");
        terminal_print("logout     log out and select account");
    terminal_print("lsblk      list storage devices");
    terminal_print("ram        show memory usage");
    }
    else if(!strcmp_local(command, "lsblk"))
    {
        command_lsblk();
    }
    else if(!strcmp_local(command, "ram") ||
            !strcmp_local(command, "free"))
    {
        command_ram();
    }
else if(!strcmp_local(command, "ls"))
    {
        command_ls();
    }
    else if(!strcmp_local(command, "pwd"))
    {
        command_pwd();
    }
    else if(!strcmp_local(command, "cd"))
    {
        vfs_node_t *dir;

        if(!arg[0])
            dir = vfs_get_root();
        else
            dir = resolve_path(
                fm_dir,
                arg);

        if(!dir || !dir->is_dir)
        {
            terminal_print(
                "cd: directory not found");
        }
        else
        {
            fm_dir = dir;
            fm_index = 0;
            fm_scroll = 0;
        }
    }
    else if(!strcmp_local(command, "mkdir"))
    {
        if(!arg[0])
            terminal_print(
                "mkdir: missing operand");
        else if(vfs_create_dir(
                    fm_dir,
                    arg))
            terminal_print(
                "directory created");
        else
            terminal_print(
                "mkdir: failed");
    }
    else if(!strcmp_local(command, "touch"))
    {
        if(!arg[0])
            terminal_print(
                "touch: missing operand");
        else if(vfs_create_file(
                    fm_dir,
                    arg))
            terminal_print(
                "file created");
        else
            terminal_print(
                "touch: failed");
    }
    else if(!strcmp_local(command, "cat"))
    {
        vfs_node_t *file =
            resolve_path(
                fm_dir,
                arg);

        if(!file || file->is_dir)
        {
            terminal_print(
                "cat: file not found");
        }
        else if(!vfs_read(file))
        {
            terminal_print(
                "cat: read failed");
        }
        else
        {
            terminal_print(file->data);
        }
    }
    else if(!strcmp_local(command, "rm"))
    {
        vfs_node_t *file =
            resolve_path(
                fm_dir,
                arg);

        if(!file || file->is_dir)
            terminal_print(
                "rm: file not found");
        else if(vfs_delete(file))
            terminal_print("removed");
        else
            terminal_print("rm: failed");
    }
    else if(!strcmp_local(command, "rmdir"))
    {
        vfs_node_t *dir =
            resolve_path(
                fm_dir,
                arg);

        if(!dir || !dir->is_dir)
        {
            terminal_print(
                "rmdir: directory not found");
        }
        else if(dir == vfs_get_root())
        {
            terminal_print(
                "rmdir: cannot remove root");
        }
        else if(vfs_delete(dir))
        {
            terminal_print(
                "directory removed");
        }
        else
        {
            terminal_print(
                "rmdir: directory not empty");
        }
    }
    else if(!strcmp_local(command, "fm"))
    {
        current_screen =
            SCREEN_FILEMANAGER;

        draw_file_manager();
        return;
    }
    else if(!strcmp_local(command, "open"))
    {
        vfs_node_t *file =
            resolve_path(
                fm_dir,
                arg);

        if(!file || file->is_dir)
        {
            terminal_print(
                "open: file not found");
        }
        else
        {
            editor_open_file(file);
            return;
        }
    }
    else if(!strcmp_local(command, "run"))
    {
        vfs_node_t *file =
            resolve_path(
                fm_dir,
                arg);

        if(!file || file->is_dir)
        {
            terminal_print(
                "run: file not found");
        }
        else if(!vfs_read(file))
        {
            terminal_print(
                "run: could not read file");
        }
        else if(!tex_execute(file))
        {
            terminal_print(
                "run: execution failed");
        }
    }
    else if(!strcmp_local(command, "turbix"))
    {
        char source_name[80];
        char output_name[32];
        vfs_node_t *source;
        vfs_node_t *output;

        if(!parse_compile_argument(arg, source_name))
        {
            terminal_print(
                "usage: turbix -c <file.tix>");
        }
        else
        {
            source =
                resolve_path(
                    fm_dir,
                    source_name);

            if(!source || source->is_dir)
            {
                terminal_print(
                    "turbix: source file not found");
            }
            else
            {
                make_tex_name(
                    source->name,
                    output_name);

                output =
                    vfs_find(
                        source->parent,
                        output_name);

                if(output)
                    vfs_delete(output);

                output =
                    vfs_create_file(
                        source->parent,
                        output_name);

                if(!output)
                {
                    terminal_print(
                        "turbix: cannot create output");
                }
                else if(!turbix_compile(
                            source,
                            output))
                {
                    vfs_delete(output);
                    terminal_print(
                        "turbix: compilation failed");
                }
                else
                {
                    terminal_print(
                        "compiled successfully");
                }
            }
        }
    }
    else if(!strcmp_local(command, "ver"))
    {
        terminal_print("turbOS v" TURBOS_VERSION);
    }
    else if(!strcmp_local(command, "about"))
    {
        terminal_print("turbOS");
        terminal_print(
            "Created by Turbosu Pramanik");
        terminal_print("i386 kernel");
    }
    else if(!strcmp_local(command, "clear"))
    {
        output_lines = 0;
        terminal_scroll = 0;
    }
    else if(!strcmp_local(command, "time"))
    {
        unsigned char h;
        unsigned char m;
        unsigned char sec;
        char text[9];

        rtc_get_time(&h, &m, &sec);
        text[0] = '0' + h / 10;
        text[1] = '0' + h % 10;
        text[2] = ':';
        text[3] = '0' + m / 10;
        text[4] = '0' + m % 10;
        text[5] = ':';
        text[6] = '0' + sec / 10;
        text[7] = '0' + sec % 10;
        text[8] = 0;
        terminal_print(text);
    }
    else if(!strcmp_local(command, "fetch"))
    {
        command_fetch();
    }
    else if(!strcmp_local(command, "logout"))
    {
        int login_ok;

        terminal_print("Logging out...");
        terminal_input[0] = 0;
        terminal_pos = 0;
        current_screen = SCREEN_TERMINAL;

        vga_console_begin();
        login_ok = accounts_boot_login();
        vga_console_end();

        if(login_ok)
        {
            current_screen = SCREEN_TERMINAL;
            fm_dir = vfs_get_root();
            fm_index = 0;
            fm_scroll = 0;
            draw_terminal();
        }
        else
        {
            terminal_print("logout: login failed");
            current_screen = SCREEN_TERMINAL;
            draw_terminal();
        }
        return;
    }
    else if(!strcmp_local(command, "reboot"))
    {
        power_reboot();
    }
    else if(!strcmp_local(command, "powoff"))
    {
        vga_clear(0x00);

        vga_print(
            "Shutting down turbOS...",
            10,
            25,
            0x0F);

        power_shutdown();
    }
    else if(command[0])
    {
        terminal_print(
            "turbCMD!: command not found");
    }
}

#define TM_BG GFX_RGB(24, 24, 24)
#define TM_BAR GFX_RGB(45, 45, 45)
#define TM_LINE GFX_RGB(70, 70, 70)
#define TM_TEXT GFX_RGB(210, 210, 210)
#define TM_DIM GFX_RGB(140, 140, 140)
#define TM_PROMPT GFX_RGB(80, 220, 100)
#define TM_BTN GFX_RGB(62, 62, 62)
#define TM_TOOL_H 38
#define TM_ROW_H 18
#define TM_PAD 10
#define TM_SCROLL_W 14
#define TM_BTN_W 80
#define TM_BTN_H 26

static int gui_rows;

static int history_rows(void)
{
    return gui_rows > 1 ? gui_rows - 1 : 23;
}

static int prompt_len_of(const char *line, const char *prompt)
{
    int i = 0;

    while(prompt[i])
    {
        if(line[i] != prompt[i])
            return 0;

        i++;
    }

    return i;
}

static const uint32_t term_palette[9] =
{
    TM_TEXT, GFX_RGB(80, 200, 220), GFX_RGB(230, 200, 80), TM_PROMPT,
    GFX_RGB(230, 90, 90), GFX_RGB(200, 110, 220), GFX_RGB(90, 140, 240), TM_TEXT,
    GFX_RGB(255, 255, 255)
};

static void term_line(int x, int y, const char *s, int plen, int cols)
{
    uint32_t fg = TM_TEXT;
    int col = 0;

    for(int i = 0; s[i] && col < cols; i++)
    {
        if((unsigned char)s[i] < 9)
        {
            fg = term_palette[(int)s[i]];
            continue;
        }

        gfx_draw_char(x + col * 8, y, s[i], i < plen ? TM_PROMPT : fg);
        col++;
    }
}

static void terminal_draw(int x, int y, int w, int h)
{
    char prompt[TERMINAL_WIDTH];
    char path[TERMINAL_WIDTH];
    int top = y + TM_TOOL_H;
    int body = top + TM_PAD;
    int rows = (h - TM_TOOL_H - 2 * TM_PAD) / TM_ROW_H;
    int cols = (w - 2 * TM_PAD - TM_SCROLL_W) / 8;
    int hist;
    int start;
    int shown;
    int plen;

    gui_rows = rows < 2 ? 2 : rows;
    hist = history_rows();
    start = output_lines - hist - terminal_scroll;

    if(start < 0)
        start = 0;

    build_prompt(prompt);
    build_path(path);
    plen = strlen_local(prompt);

    gfx_fill_rect(x, y, w, h, TM_BG);
    gfx_fill_rect(x, y, w, TM_TOOL_H, TM_BAR);
    gfx_hline(x, top - 1, w, TM_LINE);
    gfx_fill_rect(x + 8, y + 6, TM_BTN_W, TM_BTN_H, TM_BTN);
    gfx_rect_outline(x + 8, y + 6, TM_BTN_W, TM_BTN_H, TM_LINE);
    gfx_draw_text(x + 8 + (TM_BTN_W - gfx_text_width("Clear")) / 2,
                  y + 11, "Clear", TM_TEXT);
    gfx_draw_text(x + TM_BTN_W + 22, y + 11, path, TM_DIM);

    for(shown = 0; shown < hist && start + shown < output_lines; shown++)
    {
        const char *line = terminal_output[start + shown];

        term_line(x + TM_PAD, body + shown * TM_ROW_H, line,
                  prompt_len_of(line, prompt), cols);
    }

    if(terminal_scroll == 0)
    {
        int py = body + shown * TM_ROW_H;
        int caret = plen + 1 + terminal_pos;

        term_line(x + TM_PAD, py, prompt, plen, cols);

        if(cols > plen + 1)
            term_line(x + TM_PAD + (plen + 1) * 8, py,
                      terminal_input, 0, cols - plen - 1);

        if(caret < cols)
            gfx_fill_rect(x + TM_PAD + caret * 8, py, 8, 16, TM_TEXT);
    }

    gfx_fill_rect(x + w - TM_SCROLL_W, top, TM_SCROLL_W, h - TM_TOOL_H, TM_BAR);

    if(output_lines > hist)
    {
        int track = h - TM_TOOL_H;
        int span = output_lines - hist;
        int thumb = track * hist / output_lines;
        int pos;

        if(thumb < 16)
            thumb = 16;

        pos = (track - thumb) * (span - terminal_scroll) / span;

        gfx_fill_rect(x + w - TM_SCROLL_W + 2, top + pos,
                      TM_SCROLL_W - 4, thumb, TM_DIM);
    }
}

static int in_box(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && my >= y && mx < x + w && my < y + h;
}

static void terminal_mouse(int x, int y, int w, int h, int mx, int my)
{
    int top = y + TM_TOOL_H;
    int hist = history_rows();
    int span = output_lines > hist ? output_lines - hist : 0;

    if(in_box(mx, my, x + 8, y + 6, TM_BTN_W, TM_BTN_H))
    {
        output_lines = 0;
        terminal_scroll = 0;
    }
    else if(in_box(mx, my, x + w - TM_SCROLL_W, top, TM_SCROLL_W, h - TM_TOOL_H))
    {
        if(my < top + (h - TM_TOOL_H) / 2)
            terminal_scroll += hist;
        else
            terminal_scroll -= hist;

        if(terminal_scroll > span)
            terminal_scroll = span;

        if(terminal_scroll < 0)
            terminal_scroll = 0;
    }
}

void handle_terminal_key(int key)
{
    if(key == KEY_CTRL)
    {
        ctrl_pressed = 1;
        return;
    }

    if(ctrl_pressed)
        return;

    if(key == KEY_UP)
    {
        if(output_lines > history_rows() &&
           terminal_scroll <
           output_lines - history_rows())
        {
            terminal_scroll++;
        }

        draw_terminal();
        return;
    }

    if(key == KEY_DOWN)
    {
        if(terminal_scroll > 0)
            terminal_scroll--;

        draw_terminal();
        return;
    }

    if(key == KEY_BACKSPACE)
    {
        if(terminal_pos > 0)
        {
            terminal_pos--;

            terminal_input[
                terminal_pos] = 0;
        }

        draw_terminal();
        return;
    }

    if(key == KEY_ENTER)
    {
        if(terminal_pos)
        {
            terminal_add_command();
            execute_command();
        }
        else
        {
            char prompt[TERMINAL_WIDTH];

            build_prompt(prompt);
            terminal_push_line(prompt);
        }

        terminal_pos = 0;
        terminal_input[0] = 0;
        terminal_scroll = 0;

        if(current_screen ==
           SCREEN_TERMINAL)
            draw_terminal();

        return;
    }

    if(key >= 32 && key <= 126)
    {
        if(terminal_pos < 79)
        {
            terminal_input[
                terminal_pos++] =
                (char)key;

            terminal_input[
                terminal_pos] = 0;
        }

        draw_terminal();
    }
}

static void terminal_icon(int x, int y, int s)
{
        app_icon_box(x, y, s, 2, 5, 28, 22, GFX_RGB(31, 31, 31));
        app_icon_box(x, y, s, 2, 5, 28, 4, GFX_RGB(230, 230, 230));
        gfx_line(x + app_icon_sc(7, s), y + app_icon_sc(12, s),
                 x + app_icon_sc(12, s), y + app_icon_sc(16, s), GFX_RGB(22, 198, 12));
        gfx_line(x + app_icon_sc(12, s), y + app_icon_sc(16, s),
                 x + app_icon_sc(7, s), y + app_icon_sc(20, s), GFX_RGB(22, 198, 12));
        if(s >= 24)
        {
            gfx_line(x + app_icon_sc(7, s) + 1, y + app_icon_sc(12, s),
                     x + app_icon_sc(12, s) + 1, y + app_icon_sc(16, s), GFX_RGB(22, 198, 12));
            gfx_line(x + app_icon_sc(12, s) + 1, y + app_icon_sc(16, s),
                     x + app_icon_sc(7, s) + 1, y + app_icon_sc(20, s), GFX_RGB(22, 198, 12));
        }
        app_icon_box(x, y, s, 15, 20, 8, 2, GFX_RGB(204, 204, 204));
}

const app_t terminal_app =
{
    SCREEN_TERMINAL, "Terminal", "Terminal",
    terminal_draw, terminal_mouse,
    handle_terminal_key, terminal_icon,
    0, 0, APP_TICK_VISIBLE,
    0, draw_terminal
};
