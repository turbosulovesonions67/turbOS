#include "ui/terminal.h"
#include "kernel/state.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "power/power.h"
#include "ui/home.h"
#include "ui/filemanager.h"
#include "ui/editor.h"
#include "fs/vfs.h"

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
        terminal_print("clear      clear terminal");
        terminal_print("time       show home clock");
        terminal_print("about      show system information");
        terminal_print("ver        show version");
        terminal_print("fetch      show system info");
        terminal_print("reboot     reboot");
        terminal_print("powoff     power off");
        terminal_print("exit       return home");
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
    else if(!strcmp_local(command, "ver"))
    {
        terminal_print("turbOS v0.2");
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
        terminal_print(
            "Use the Home clock.");
    }
    else if(!strcmp_local(command, "fetch"))
    {
        terminal_print("ttt        \\\\\\");
        terminal_print("ttt          \\\\\\");
        terminal_print("tttttttt      \\\\\\");
        terminal_print("ttt            \\\\\\");
        terminal_print("ttt              \\\\\\");
        terminal_print("  tttttt          \\\\\\");
        terminal_print("turbOS v0.2");
        terminal_print("Kernel : TASK-32bit");
        terminal_print("Arch   : i386");
        terminal_print("Shell  : turbCMD!");
    }
    else if(!strcmp_local(command, "exit"))
    {
        current_screen =
            SCREEN_HOME;

        draw_home();
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
    else if(!strcmp_local(command, "bg"))
    {
        if(!strcmp_local(arg, "-c"))
        {
            home_color = 0x3F;

            terminal_print(
                "Background changed.");
        }
        else
        {
            terminal_print(
                "bg: unknown option");
        }
    }
    else if(command[0])
    {
        terminal_print(
            "turbCMD!: command not found");
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
        if(output_lines > 23 &&
           terminal_scroll <
           output_lines - 23)
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
