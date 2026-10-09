#include "../fs/vfs.h"
#include "../ui/terminal.h"
#include "turbix.h"

#define TEX_OP_HALT 0x00
#define TEX_OP_TEXT 0x01
#define TEX_OP_BEEP 0x02

static int string_equal(const char *a, const char *b)
{
    while(*a && *b && *a == *b)
    {
        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}

static int starts_with(const char *text, const char *prefix)
{
    while(*prefix)
    {
        if(*text++ != *prefix++)
            return 0;
    }

    return 1;
}

static int string_length(const char *s)
{
    int length = 0;

    while(s[length])
        length++;

    return length;
}

static void write_u32(
    char *data,
    unsigned int *offset,
    unsigned int value)
{
    data[(*offset)++] = value & 0xFF;
    data[(*offset)++] = (value >> 8) & 0xFF;
    data[(*offset)++] = (value >> 16) & 0xFF;
    data[(*offset)++] = (value >> 24) & 0xFF;
}

static int emit_text(
    char *output,
    unsigned int *offset,
    const char *line)
{
    const char *start = line + 6;
    const char *end = start;
    unsigned int length;

    while(*end && *end != '"')
        end++;

    if(*end != '"')
        return 0;

    length = (unsigned int)(end - start);

    if(*offset + 5 + length > VFS_NODE_DATA)
        return 0;

    output[(*offset)++] = TEX_OP_TEXT;

    write_u32(
        output,
        offset,
        length);

    for(unsigned int i = 0; i < length; i++)
        output[(*offset)++] = start[i];

    return 1;
}

static int parse_beep(
    char *line,
    unsigned int *frequency,
    unsigned int *milliseconds)
{
    unsigned int a = 0;
    unsigned int b = 0;
    int state = 0;

    line += 5;

    if(*line != '(')
        return 0;

    line++;

    while(*line >= '0' && *line <= '9')
    {
        a = a * 10 + (*line - '0');
        line++;
        state = 1;
    }

    if(!state || *line != ',')
        return 0;

    line++;
    state = 0;

    while(*line >= '0' && *line <= '9')
    {
        b = b * 10 + (*line - '0');
        line++;
        state = 1;
    }

    if(!state || *line != ')')
        return 0;

    *frequency = a;
    *milliseconds = b;

    return 1;
}

static char *next_line(
    char *text,
    char *line)
{
    int i = 0;

    while(text[i] && text[i] != '\n')
    {
        line[i] = text[i];
        i++;
    }

    line[i] = 0;

    if(text[i] == '\n')
        return text + i + 1;

    return text + i;
}

static char *trim(char *text)
{
    while(*text == ' ' || *text == '\t')
        text++;

    return text;
}

static void remove_cr(char *line)
{
    int length = string_length(line);

    if(length > 0 && line[length - 1] == '\r')
        line[length - 1] = 0;
}

int turbix_compile(
    vfs_node_t *source,
    vfs_node_t *output)
{
    char line[256];
    char *cursor;
    unsigned int offset = 4;
    unsigned int frequency;
    unsigned int milliseconds;
    int inside_main = 0;
    int saw_main = 0;
    int saw_halt = 0;

    if(!source || !output || source->is_dir || output->is_dir)
        return 0;

    if(!vfs_read(source))
    {
        terminal_print("turbix: source read failed");
        return 0;
    }

    output->data[0] = 'T';
    output->data[1] = 'E';
    output->data[2] = 'X';
    output->data[3] = '1';

    write_u32(
        output->data,
        &offset,
        1);

    cursor = source->data;

    while(*cursor)
    {
        cursor = next_line(cursor, line);
        remove_cr(line);

        {
            char *text = trim(line);

            if(string_equal(text, "program start"))
                continue;

            if(string_equal(text, "program end"))
                continue;

            if(string_equal(text, "main():"))
            {
                inside_main = 1;
                saw_main = 1;
                continue;
            }

            if(string_equal(text, "loop end"))
            {
                if(!inside_main)
                    return 0;

                output->data[offset++] = TEX_OP_HALT;
                saw_halt = 1;
                inside_main = 0;
                continue;
            }

            if(!inside_main)
                continue;

            if(starts_with(text, "text(\""))
            {
                if(!emit_text(
                        output->data,
                        &offset,
                        text))
                    return 0;

                continue;
            }

            if(starts_with(text, "beep("))
            {
                if(!parse_beep(
                        text,
                        &frequency,
                        &milliseconds))
                    return 0;

                output->data[offset++] = TEX_OP_BEEP;

                write_u32(
                    output->data,
                    &offset,
                    frequency);

                write_u32(
                    output->data,
                    &offset,
                    milliseconds);

                continue;
            }

            if(*text)
                return 0;
        }
    }

    if(!saw_main)
    {
        terminal_print("turbix: main missing");
        return 0;
    }

    if(!saw_halt)
    {
        terminal_print("turbix: loop end missing");
        return 0;
    }

    output->size = offset;

    if(!vfs_write(output))
    {
        terminal_print("turbix: output write failed");
        return 0;
    }

    return 1;
}
