#include "fat32.h"
#include "../drivers/ata.h"

static unsigned short rd16(const unsigned char *p)
{
    return (unsigned short)p[0] |
           ((unsigned short)p[1] << 8);
}

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static void wr16(unsigned char *p, unsigned short v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void wr32(unsigned char *p, unsigned int v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static int is_eoc(unsigned int value)
{
    return value >= 0x0FFFFFF8;
}

static void make_83(
    const char *name,
    unsigned char out[11])
{
    int i;
    int j;
    int dot = -1;

    for(i = 0; i < 11; i++)
        out[i] = ' ';

    for(i = 0; name[i]; i++)
    {
        if(name[i] == '.')
        {
            dot = i;
            break;
        }
    }

    if(dot < 0)
        dot = i;

    j = 0;

    for(i = 0; i < dot && j < 8; i++)
    {
        char c = name[i];

        if(c >= 'a' && c <= 'z')
            c -= 32;

        out[j++] = (unsigned char)c;
    }

    if(name[dot] == '.')
    {
        j = 8;

        for(i = dot + 1; name[i] && j < 11; i++)
        {
            char c = name[i];

            if(c >= 'a' && c <= 'z')
                c -= 32;

            out[j++] = (unsigned char)c;
        }
    }
}

static int name_matches(
    const unsigned char *entry,
    const char *name)
{
    unsigned char wanted[11];

    make_83(name, wanted);

    for(int i = 0; i < 11; i++)
    {
        if(entry[i] != wanted[i])
            return 0;
    }

    return 1;
}

int fat32_mount(
    fat32_fs_t *fs,
    unsigned int partition_lba)
{
    unsigned char sector[512];

    if(!fs)
        return 0;

    if(!ata_read_sector(partition_lba, sector))
        return 0;

    if(sector[510] != 0x55 ||
       sector[511] != 0xAA)
        return 0;

    if(sector[0x52] != 'F' ||
       sector[0x53] != 'A' ||
       sector[0x54] != 'T' ||
       sector[0x55] != '3' ||
       sector[0x56] != '2')
        return 0;

    unsigned int bytes_per_sector = rd16(&sector[11]);
    unsigned int sectors_per_cluster = sector[13];
    unsigned int reserved = rd16(&sector[14]);
    unsigned int fats = sector[16];
    unsigned int sectors_per_fat = rd32(&sector[36]);
    unsigned int root_cluster = rd32(&sector[44]);

    if(bytes_per_sector != 512 ||
       sectors_per_cluster == 0 ||
       fats == 0 ||
       sectors_per_fat == 0 ||
       root_cluster < 2)
        return 0;

    fs->mounted = 1;
    fs->partition_lba = partition_lba;
    fs->reserved_sectors = reserved;
    fs->fats = fats;
    fs->sectors_per_fat = sectors_per_fat;
    fs->sectors_per_cluster = sectors_per_cluster;
    fs->root_cluster = root_cluster;

    fs->fat_lba =
        partition_lba + reserved;

    fs->data_lba =
        partition_lba +
        reserved +
        fats * sectors_per_fat;

    return 1;
}

unsigned int fat32_cluster_lba(
    const fat32_fs_t *fs,
    unsigned int cluster)
{
    if(!fs ||
       !fs->mounted ||
       cluster < 2)
        return 0;

    return fs->data_lba +
           (cluster - 2) *
           fs->sectors_per_cluster;
}

unsigned int fat32_next_cluster(
    const fat32_fs_t *fs,
    unsigned int cluster)
{
    unsigned char sector[512];

    if(!fs || !fs->mounted || cluster < 2)
        return 0;

    unsigned int byte_offset = cluster * 4;
    unsigned int lba =
        fs->fat_lba +
        byte_offset / 512;

    unsigned int offset =
        byte_offset % 512;

    if(!ata_read_sector(lba, sector))
        return 0;

    return rd32(&sector[offset]) & 0x0FFFFFFF;
}

int fat32_set_cluster(
    const fat32_fs_t *fs,
    unsigned int cluster,
    unsigned int value)
{
    unsigned char sector[512];

    if(!fs || !fs->mounted || cluster < 2)
        return 0;

    value &= 0x0FFFFFFF;

    unsigned int byte_offset = cluster * 4;
    unsigned int sector_index =
        byte_offset / 512;
    unsigned int offset =
        byte_offset % 512;

    for(unsigned int fat = 0; fat < fs->fats; fat++)
    {
        unsigned int lba =
            fs->fat_lba +
            fat * fs->sectors_per_fat +
            sector_index;

        if(!ata_read_sector(lba, sector))
            return 0;

        wr32(&sector[offset], value);

        if(!ata_write_sector(lba, sector))
            return 0;
    }

    return 1;
}

unsigned int fat32_alloc_cluster(
    const fat32_fs_t *fs)
{
    if(!fs || !fs->mounted)
        return 0;

    unsigned int max_cluster =
        fs->sectors_per_fat *
        512 / 4;

    for(unsigned int cluster = 3;
        cluster < max_cluster;
        cluster++)
    {
        if(fat32_next_cluster(fs, cluster) == 0)
        {
            if(!fat32_set_cluster(
                    fs,
                    cluster,
                    0x0FFFFFFF))
                return 0;

            unsigned char zero[512];

            for(int i = 0; i < 512; i++)
                zero[i] = 0;

            for(unsigned int s = 0;
                s < fs->sectors_per_cluster;
                s++)
            {
                if(!ata_write_sector(
                        fat32_cluster_lba(fs, cluster) + s,
                        zero))
                    return 0;
            }

            return cluster;
        }
    }

    return 0;
}

int fat32_free_chain(
    const fat32_fs_t *fs,
    unsigned int cluster)
{
    if(!fs || !fs->mounted)
        return 0;

    while(cluster >= 2)
    {
        unsigned int next =
            fat32_next_cluster(fs, cluster);

        if(!fat32_set_cluster(fs, cluster, 0))
            return 0;

        if(is_eoc(next) || next == 0)
            break;

        cluster = next;
    }

    return 1;
}

int fat32_read_file(
    const fat32_fs_t *fs,
    unsigned int first_cluster,
    unsigned char *buffer,
    unsigned int max_size,
    unsigned int *size)
{
    if(!fs || !fs->mounted ||
       !buffer || !size)
        return 0;

    *size = 0;

    if(first_cluster < 2)
        return 1;

    unsigned int cluster = first_cluster;
    unsigned char sector[512];

    while(cluster >= 2 &&
          *size < max_size)
    {
        unsigned int base =
            fat32_cluster_lba(fs, cluster);

        for(unsigned int s = 0;
            s < fs->sectors_per_cluster &&
            *size < max_size;
            s++)
        {
            if(!ata_read_sector(base + s, sector))
                return 0;

            unsigned int copy = 512;

            if(copy > max_size - *size)
                copy = max_size - *size;

            for(unsigned int i = 0; i < copy; i++)
                buffer[*size + i] = sector[i];

            *size += copy;
        }

        unsigned int next =
            fat32_next_cluster(fs, cluster);

        if(is_eoc(next) || next == 0)
            break;

        cluster = next;
    }

    return 1;
}

int fat32_write_file(
    const fat32_fs_t *fs,
    unsigned int *first_cluster,
    const unsigned char *buffer,
    unsigned int size)
{
    if(!fs || !fs->mounted ||
       !first_cluster)
        return 0;

    if(*first_cluster >= 2)
    {
        if(!fat32_free_chain(
                fs,
                *first_cluster))
            return 0;

        *first_cluster = 0;
    }

    if(size == 0)
        return 1;

    unsigned int bytes_per_cluster =
        fs->sectors_per_cluster * 512;

    unsigned int needed =
        (size + bytes_per_cluster - 1) /
        bytes_per_cluster;

    unsigned int first = 0;
    unsigned int previous = 0;
    unsigned int remaining = size;
    unsigned int position = 0;

    for(unsigned int n = 0; n < needed; n++)
    {
        unsigned int cluster =
            fat32_alloc_cluster(fs);

        if(!cluster)
        {
            if(first)
                fat32_free_chain(fs, first);

            return 0;
        }

        if(!first)
            first = cluster;

        if(previous)
        {
            if(!fat32_set_cluster(
                    fs,
                    previous,
                    cluster))
            {
                fat32_free_chain(fs, first);
                return 0;
            }
        }

        unsigned int base =
            fat32_cluster_lba(fs, cluster);

        for(unsigned int s = 0;
            s < fs->sectors_per_cluster;
            s++)
        {
            unsigned char sector[512];

            for(int i = 0; i < 512; i++)
                sector[i] = 0;

            unsigned int copy = remaining;

            if(copy > 512)
                copy = 512;

            for(unsigned int i = 0; i < copy; i++)
                sector[i] = buffer[position + i];

            if(!ata_write_sector(
                    base + s,
                    sector))
            {
                fat32_free_chain(fs, first);
                return 0;
            }

            position += copy;
            remaining -= copy;
        }

        previous = cluster;
    }

    *first_cluster = first;

    return 1;
}

static int find_free_directory_slot(
    const fat32_fs_t *fs,
    unsigned int dir_cluster,
    unsigned int *entry_lba,
    unsigned int *entry_offset)
{
    unsigned int cluster = dir_cluster;

    while(cluster >= 2)
    {
        unsigned int base =
            fat32_cluster_lba(fs, cluster);

        for(unsigned int s = 0;
            s < fs->sectors_per_cluster;
            s++)
        {
            unsigned char sector[512];

            if(!ata_read_sector(base + s, sector))
                return 0;

            for(unsigned int off = 0;
                off < 512;
                off += 32)
            {
                if(sector[off] == 0x00 ||
                   sector[off] == 0xE5)
                {
                    *entry_lba = base + s;
                    *entry_offset = off;
                    return 1;
                }
            }
        }

        unsigned int next =
            fat32_next_cluster(fs, cluster);

        if(is_eoc(next) || next == 0)
            break;

        cluster = next;
    }

    unsigned int new_cluster =
        fat32_alloc_cluster(fs);

    if(!new_cluster)
        return 0;

    if(!fat32_set_cluster(
            fs,
            cluster,
            new_cluster))
        return 0;

    *entry_lba =
        fat32_cluster_lba(fs, new_cluster);

    *entry_offset = 0;

    return 1;
}

int fat32_create_entry(
    const fat32_fs_t *fs,
    unsigned int dir_cluster,
    const char *name,
    int is_dir,
    unsigned int first_cluster,
    unsigned int size,
    unsigned int *entry_lba,
    unsigned int *entry_offset)
{
    unsigned char sector[512];
    unsigned char short_name[11];

    if(!fs || !fs->mounted ||
       dir_cluster < 2 ||
       !name ||
       !entry_lba ||
       !entry_offset)
        return 0;

    make_83(name, short_name);

    if(!find_free_directory_slot(
            fs,
            dir_cluster,
            entry_lba,
            entry_offset))
        return 0;

    if(!ata_read_sector(*entry_lba, sector))
        return 0;

    for(int i = 0; i < 32; i++)
        sector[*entry_offset + i] = 0;

    for(int i = 0; i < 11; i++)
        sector[*entry_offset + i] = short_name[i];

    sector[*entry_offset + 11] =
        is_dir ? 0x10 : 0x20;

    wr16(
        &sector[*entry_offset + 20],
        (unsigned short)(first_cluster >> 16));

    wr16(
        &sector[*entry_offset + 26],
        (unsigned short)(first_cluster & 0xFFFF));

    wr32(
        &sector[*entry_offset + 28],
        size);

    return ata_write_sector(*entry_lba, sector);
}

int fat32_update_entry(
    const fat32_fs_t *fs,
    unsigned int entry_lba,
    unsigned int entry_offset,
    unsigned int first_cluster,
    unsigned int size,
    int is_dir,
    const char *name)
{
    unsigned char sector[512];
    unsigned char short_name[11];

    if(!fs || !fs->mounted ||
       entry_offset >= 512 ||
       entry_offset % 32)
        return 0;

    if(!ata_read_sector(entry_lba, sector))
        return 0;

    if(name)
    {
        make_83(name, short_name);

        for(int i = 0; i < 11; i++)
            sector[entry_offset + i] =
                short_name[i];
    }

    sector[entry_offset + 11] =
        is_dir ? 0x10 : 0x20;

    wr16(
        &sector[entry_offset + 20],
        (unsigned short)(first_cluster >> 16));

    wr16(
        &sector[entry_offset + 26],
        (unsigned short)(first_cluster & 0xFFFF));

    wr32(
        &sector[entry_offset + 28],
        size);

    return ata_write_sector(entry_lba, sector);
}

int fat32_delete_entry(
    const fat32_fs_t *fs,
    unsigned int entry_lba,
    unsigned int entry_offset,
    unsigned int first_cluster)
{
    unsigned char sector[512];

    if(!fs || !fs->mounted)
        return 0;

    if(!ata_read_sector(entry_lba, sector))
        return 0;

    sector[entry_offset] = 0xE5;

    if(!ata_write_sector(entry_lba, sector))
        return 0;

    if(first_cluster >= 2)
        return fat32_free_chain(
            fs,
            first_cluster);

    return 1;
}

int fat32_directory_empty(
    const fat32_fs_t *fs,
    unsigned int dir_cluster)
{
    unsigned int cluster = dir_cluster;

    while(cluster >= 2)
    {
        unsigned int base =
            fat32_cluster_lba(fs, cluster);

        for(unsigned int s = 0;
            s < fs->sectors_per_cluster;
            s++)
        {
            unsigned char sector[512];

            if(!ata_read_sector(base + s, sector))
                return 0;

            for(unsigned int off = 0;
                off < 512;
                off += 32)
            {
                unsigned char first =
                    sector[off];

                if(first == 0x00)
                    return 1;

                if(first == 0xE5)
                    continue;

                unsigned char attr =
                    sector[off + 11];

                if(attr == 0x0F)
                    continue;

                if(sector[off] == '.' ||
                   sector[off] == '.')
                    continue;

                return 0;
            }
        }

        unsigned int next =
            fat32_next_cluster(fs, cluster);

        if(is_eoc(next) || next == 0)
            break;

        cluster = next;
    }

    return 1;
}
