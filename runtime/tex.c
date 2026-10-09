#include <stddef.h>
#include "../fs/vfs.h"
#include "../drivers/speaker.h"
#include "../ui/terminal.h"
#include "tex.h"

#define TEX_OP_HALT 0x00
#define TEX_OP_TEXT 0x01
#define TEX_OP_BEEP 0x02

static unsigned int read_u32(const unsigned char *data, unsigned int *offset, unsigned int size)
{
    unsigned int value;

    if(*offset > size || size - *offset < 4)
        return 0;

    value = (unsigned int)data[*offset] |
            ((unsigned int)data[*offset + 1] << 8) |
            ((unsigned int)data[*offset + 2] << 16) |
            ((unsigned int)data[*offset + 3] << 24);

    *offset += 4;
    return value;
}

int tex_execute(vfs_node_t *node)
{
    unsigned char *data;
    unsigned int offset = 4;
    unsigned int version;

    if(!node || node->is_dir)
    {
        terminal_print("TEX: invalid file");
        return 0;
    }

    if(node->size < 8)
    {
        terminal_print("TEX: file too small");
        return 0;
    }

    data = (unsigned char *)node->data;

    if(data[0] != 'T' || data[1] != 'E' ||
       data[2] != 'X' || data[3] != '1')
    {
        terminal_print("TEX: bad magic");
        return 0;
    }

    version = read_u32(data, &offset, node->size);

    if(version != 1)
    {
        terminal_print("TEX: bad version");
        return 0;
    }

    while(offset < node->size)
    {
        unsigned int length;
        unsigned int frequency;
        unsigned int milliseconds;

        switch(data[offset++])
        {
            case TEX_OP_HALT:
                return 1;

            case TEX_OP_TEXT:
                length = read_u32(data, &offset, node->size);

                if(offset > node->size || length > node->size - offset)
                {
                    terminal_print("TEX: bad text length");
                    return 0;
                }

                {
                    char text[1024];
                    unsigned int n = length;

                    if(n >= sizeof(text))
                        n = sizeof(text) - 1;

                    for(unsigned int i = 0; i < n; i++)
                        text[i] = (char)data[offset + i];

                    text[n] = 0;
                    terminal_print(text);
                }

                offset += length;
                break;

            case TEX_OP_BEEP:
                if(offset > node->size || node->size - offset < 8)
                {
                    terminal_print("TEX: bad beep data");
                    return 0;
                }

                frequency = read_u32(data, &offset, node->size);
                milliseconds = read_u32(data, &offset, node->size);
                speaker_play(frequency, milliseconds);
                break;

            default:
                terminal_print("TEX: unknown opcode");
                return 0;
        }
    }

    terminal_print("TEX: missing halt");
    return 0;
}
