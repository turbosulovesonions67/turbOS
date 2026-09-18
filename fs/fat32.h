#ifndef FAT32_H
#define FAT32_H

typedef struct
{
    int mounted;
    unsigned int partition_lba;
    unsigned int fat_lba;
    unsigned int data_lba;
    unsigned int reserved_sectors;
    unsigned int fats;
    unsigned int sectors_per_fat;
    unsigned int sectors_per_cluster;
    unsigned int root_cluster;
} fat32_fs_t;

int fat32_mount(fat32_fs_t *fs, unsigned int partition_lba);

unsigned int fat32_cluster_lba(
    const fat32_fs_t *fs,
    unsigned int cluster);

unsigned int fat32_next_cluster(
    const fat32_fs_t *fs,
    unsigned int cluster);

int fat32_set_cluster(
    const fat32_fs_t *fs,
    unsigned int cluster,
    unsigned int value);

unsigned int fat32_alloc_cluster(
    const fat32_fs_t *fs);

int fat32_free_chain(
    const fat32_fs_t *fs,
    unsigned int cluster);

int fat32_read_file(
    const fat32_fs_t *fs,
    unsigned int first_cluster,
    unsigned char *buffer,
    unsigned int max_size,
    unsigned int *size);

int fat32_write_file(
    const fat32_fs_t *fs,
    unsigned int *first_cluster,
    const unsigned char *buffer,
    unsigned int size);

int fat32_create_entry(
    const fat32_fs_t *fs,
    unsigned int dir_cluster,
    const char *name,
    int is_dir,
    unsigned int first_cluster,
    unsigned int size,
    unsigned int *entry_lba,
    unsigned int *entry_offset);

int fat32_update_entry(
    const fat32_fs_t *fs,
    unsigned int entry_lba,
    unsigned int entry_offset,
    unsigned int first_cluster,
    unsigned int size,
    int is_dir,
    const char *name);

int fat32_delete_entry(
    const fat32_fs_t *fs,
    unsigned int entry_lba,
    unsigned int entry_offset,
    unsigned int first_cluster);

int fat32_directory_empty(
    const fat32_fs_t *fs,
    unsigned int dir_cluster);

#endif
