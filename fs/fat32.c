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

static void make_83(const char *name, unsigned char out[11])
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

static unsigned int fat_entry_lba(
    const fat32_fs_t *fs,
    unsigned int cluster)
{
    return fs->fat_lba +
           (cluster * 4) / 512;
}

static unsigned int fat_entry_offset(
    unsigned int cluster)
{
    return (cluster * 4) % 512;
}

static unsigned int read_fat_entry(
    const fat32_fs_t *fs,
    unsigned int cluster)
{
    unsigned int lba;
    unsigned int offset;
    unsigned char sector[512];

    lba = fat_entry_lba(fs, cluster);
    offset = fat_entry_offset(cluster);

    if(!ata_read_sector(lba, sector))
        return 0;

    return rd32(&sector[offset]) &
           0x0FFFFFFF;
}

static void zero_buffer(unsigned char *buffer)
{
    for(int i = 0; i < 512; i++)
        buffer[i] = 0;
}

static int write_fat_entry(
    const fat32_fs_t *fs,
    unsigned int cluster,
    unsigned int value)
{
    unsigned int lba;
    unsigned int offset;
    unsigned char sector[512];

    value &= 0x0FFFFFFF;

    lba = fat_entry_lba(fs, cluster);
    offset = fat_entry_offset(cluster);

    for(unsigned int fat = 0;
        fat < fs->fats;
        fat++)
    {
        unsigned int fat_lba =
            lba + fat * fs->sectors_per_fat;

        if(!ata_read_sector(
                fat_lba,
                sector))
            return 0;

        wr32(&sector[offset], value);

        if(!ata_write_sector(
                fat_lba,
                sector))
            return 0;
    }

    return 1;
}

int fat32_mount(fat32_fs_t *fs, unsigned int partition_lba)
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
    unsigned int total_sectors =
        rd16(&sector[19]);

    if(total_sectors == 0)
        total_sectors = rd32(&sector[32]);
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
    fs->total_sectors = total_sectors;

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
    if(!fs || !fs->mounted || cluster < 2)
        return 0;

    return fs->data_lba +
           (cluster - 2) *
           fs->sectors_per_cluster;
}

unsigned int fat32_next_cluster(
    const fat32_fs_t *fs,
    unsigned int cluster)
{
    if(!fs || !fs->mounted || cluster < 2)
        return 0;

    return read_fat_entry(fs, cluster);
}

int fat32_set_cluster(
    const fat32_fs_t *fs,
    unsigned int cluster,
    unsigned int value)
{
    if(!fs || !fs->mounted || cluster < 2)
        return 0;

    return write_fat_entry(fs, cluster, value);
}

unsigned int fat32_alloc_cluster(
    const fat32_fs_t *fs)
{
    unsigned int max_cluster;

    if(!fs || !fs->mounted)
        return 0;

    max_cluster =
        fs->sectors_per_fat * 512 / 4;

    for(unsigned int cluster = 3;
        cluster < max_cluster;
        cluster++)
    {
        if(fat32_next_cluster(fs, cluster) == 0)
        {
            unsigned char zero[512];

            if(!fat32_set_cluster(
                    fs,
                    cluster,
                    0x0FFFFFFF))
                return 0;

            zero_buffer(zero);

            for(unsigned int s = 0;
                s < fs->sectors_per_cluster;
                s++)
            {
                if(!ata_write_sector(
                        fat32_cluster_lba(fs, cluster) + s,
                        zero))
                {
                    fat32_set_cluster(fs, cluster, 0);
                    return 0;
                }
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

        if(previous &&
           !fat32_set_cluster(
                fs,
                previous,
                cluster))
        {
            fat32_free_chain(fs, first);
            return 0;
        }

        unsigned int base =
            fat32_cluster_lba(fs, cluster);

        for(unsigned int s = 0;
            s < fs->sectors_per_cluster;
            s++)
        {
            unsigned char sector[512];

            zero_buffer(sector);

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
    {
        fat32_free_chain(fs, new_cluster);
        return 0;
    }

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

    if(!ata_write_sector(*entry_lba, sector))
        return 0;

    if(is_dir && first_cluster >= 2)
    {
        unsigned char dir_sector[512];

        for(unsigned int s = 0;
            s < fs->sectors_per_cluster;
            s++)
        {
            if(!ata_read_sector(
                    fat32_cluster_lba(fs, first_cluster) + s,
                    dir_sector))
                return 0;

            if(s == 0)
            {
                for(int i = 0; i < 512; i++)
                    dir_sector[i] = 0;

                for(int i = 0; i < 11; i++)
                    dir_sector[i] = ' ';

                dir_sector[0] = '.';
                dir_sector[11] = 0x10;

                wr16(&dir_sector[20],
                     (unsigned short)(first_cluster >> 16));
                wr16(&dir_sector[26],
                     (unsigned short)(first_cluster & 0xFFFF));

                for(int i = 32; i < 43; i++)
                    dir_sector[i] = ' ';

                dir_sector[32] = '.';
                dir_sector[33] = '.';
                dir_sector[43] = 0x10;

                wr16(&dir_sector[52],
                     (unsigned short)(dir_cluster >> 16));
                wr16(&dir_sector[58],
                     (unsigned short)(dir_cluster & 0xFFFF));
            }

            if(!ata_write_sector(
                    fat32_cluster_lba(fs, first_cluster) + s,
                    dir_sector))
                return 0;
        }
    }

    return 1;
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

    if(!fs || !fs->mounted ||
       entry_offset >= 512 ||
       entry_offset % 32)
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

                if(sector[off + 11] == 0x0F)
                    continue;

                if(first == '.' &&
                   (sector[off + 1] == ' ' ||
                    sector[off + 1] == '.'))
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


int fat32_get_space(
    const fat32_fs_t *fs,
    unsigned int *total_bytes,
    unsigned int *used_bytes,
    unsigned int *free_bytes)
{
    unsigned int data_sectors;
    unsigned int cluster_count;
    unsigned int free_clusters = 0;

    if(!fs || !fs->mounted ||
       !total_bytes ||
       !used_bytes ||
       !free_bytes ||
       fs->sectors_per_cluster == 0 ||
       fs->total_sectors == 0)
        return 0;

    data_sectors =
        fs->total_sectors -
        fs->reserved_sectors -
        fs->fats * fs->sectors_per_fat;

    cluster_count =
        data_sectors /
        fs->sectors_per_cluster;

    if(cluster_count == 0)
        return 0;

    unsigned int fat_entries_per_sector = 128;
    unsigned int sectors_to_scan =
        (cluster_count + fat_entries_per_sector - 1) /
        fat_entries_per_sector;

    unsigned char sector[512];

    for(unsigned int s = 0;
        s < sectors_to_scan;
        s++)
    {
        if(!ata_read_sector(
                fs->fat_lba + s,
                sector))
            return 0;

        unsigned int entries = fat_entries_per_sector;

        if(s == sectors_to_scan - 1)
        {
            unsigned int remaining =
                cluster_count -
                s * fat_entries_per_sector;

            if(remaining < entries)
                entries = remaining;
        }

        for(unsigned int i = 0;
            i < entries;
            i++)
        {
            unsigned int cluster =
                s * fat_entries_per_sector + i + 2;

            if(cluster < 2)
                continue;

            unsigned int value =
                rd32(&sector[i * 4]) &
                0x0FFFFFFF;

            if(value == 0)
                free_clusters++;
        }
    }

    unsigned int bytes_per_cluster =
        fs->sectors_per_cluster * 512;

    *total_bytes =
        cluster_count *
        bytes_per_cluster;

    *free_bytes =
        free_clusters *
        bytes_per_cluster;

    *used_bytes =
        *total_bytes -
        *free_bytes;

    return 1;
}
