#include "../drivers/ata.h"
#include "vfs.h"

static vfs_node_t vfs_pool[VFS_MAX_NODES];
static int vfs_used;
static vfs_node_t *root;
static fat32_fs_t *vfs_fs;

static int copy_name(char *dst, const char *src)
{
    int i = 0;

    while(src[i] && i < 31)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;

    return i;
}

static vfs_node_t *vfs_alloc(void)
{
    if(vfs_used >= VFS_MAX_NODES)
        return 0;

    vfs_node_t *node =
        &vfs_pool[vfs_used++];

    for(int i = 0;
        i < (int)sizeof(vfs_node_t);
        i++)
        ((unsigned char *)node)[i] = 0;

    return node;
}

static void link_node(
    vfs_node_t *parent,
    vfs_node_t *node)
{
    node->parent = parent;

    if(!parent)
        return;

    if(!parent->child)
    {
        parent->child = node;
        return;
    }

    vfs_node_t *last = parent->child;

    while(last->next)
        last = last->next;

    last->next = node;
}

static vfs_node_t *load_node(
    vfs_node_t *parent,
    const char *name,
    int is_dir,
    unsigned int first_cluster,
    unsigned int lba,
    unsigned int offset,
    unsigned int size)
{
    vfs_node_t *node = vfs_alloc();

    if(!node)
        return 0;

    copy_name(node->name, name);

    node->is_dir = is_dir;
    node->first_cluster = first_cluster;
    node->dir_entry_lba = lba;
    node->dir_entry_offset = offset;
    node->size = size;

    link_node(parent, node);

    if(!is_dir && size)
        vfs_read(node);

    return node;
}

static void make_name(
    const unsigned char *entry,
    char *out)
{
    int p = 0;

    for(int i = 0; i < 8; i++)
    {
        if(entry[i] == ' ')
            break;

        out[p++] = entry[i];
    }

    if(entry[8] != ' ')
    {
        out[p++] = '.';

        for(int i = 8; i < 11; i++)
        {
            if(entry[i] == ' ')
                break;

            out[p++] = entry[i];
        }
    }

    out[p] = 0;

    for(int i = 0; out[i]; i++)
    {
        if(out[i] >= 'A' && out[i] <= 'Z')
            out[i] += 32;
    }
}

static int load_directory(vfs_node_t *dir)
{
    if(!vfs_fs || !dir)
        return 0;

    unsigned int cluster =
        dir->first_cluster;

    while(cluster >= 2)
    {
        unsigned int base =
            fat32_cluster_lba(
                vfs_fs,
                cluster);

        for(unsigned int s = 0;
            s < vfs_fs->sectors_per_cluster;
            s++)
        {
            unsigned char sector[512];

            if(!ata_read_sector(
                    base + s,
                    sector))
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

                if(attr & 0x08)
                    continue;

                if(sector[off] == '.' ||
                   sector[off] == '.')
                    continue;

                char name[32];
                make_name(
                    &sector[off],
                    name);

                unsigned int first_cluster =
                    ((unsigned int)(
                        sector[off + 20] |
                        ((unsigned int)
                            sector[off + 21] << 8))
                     << 16) |
                    (unsigned int)(
                        sector[off + 26] |
                        ((unsigned int)
                            sector[off + 27] << 8));

                unsigned int size =
                    (unsigned int)sector[off + 28] |
                    ((unsigned int)
                        sector[off + 29] << 8) |
                    ((unsigned int)
                        sector[off + 30] << 16) |
                    ((unsigned int)
                        sector[off + 31] << 24);

                vfs_node_t *node =
                    load_node(
                        dir,
                        name,
                        (attr & 0x10) != 0,
                        first_cluster,
                        base + s,
                        off,
                        size);

                if(!node)
                    return 0;

                if(node->is_dir &&
                   first_cluster >= 2)
                {
                    if(!load_directory(node))
                        return 0;
                }
            }
        }

        unsigned int next =
            fat32_next_cluster(
                vfs_fs,
                cluster);

        if(next < 2 ||
           next >= 0x0FFFFFF8)
            break;

        cluster = next;
    }

    return 1;
}

vfs_node_t *vfs_get_root(void)
{
    return root;
}

vfs_node_t *vfs_create(
    vfs_node_t *parent,
    const char *name,
    int is_dir)
{
    if(!parent || !name)
        return 0;

    if(vfs_find(parent, name))
        return 0;

    vfs_node_t *node = vfs_alloc();

    if(!node)
        return 0;

    copy_name(node->name, name);
    node->is_dir = is_dir;
    node->parent = parent;

    if(vfs_fs)
    {
        unsigned int cluster = 0;

        if(is_dir)
        {
            cluster =
                fat32_alloc_cluster(vfs_fs);

            if(!cluster)
                return 0;
        }

        if(!fat32_create_entry(
                vfs_fs,
                parent->first_cluster,
                name,
                is_dir,
                cluster,
                0,
                &node->dir_entry_lba,
                &node->dir_entry_offset))
        {
            if(cluster)
                fat32_free_chain(
                    vfs_fs,
                    cluster);

            return 0;
        }

        node->first_cluster = cluster;
    }

    link_node(parent, node);

    return node;
}

vfs_node_t *vfs_create_file(
    vfs_node_t *parent,
    const char *name)
{
    return vfs_create(
        parent,
        name,
        0);
}

vfs_node_t *vfs_create_dir(
    vfs_node_t *parent,
    const char *name)
{
    return vfs_create(
        parent,
        name,
        1);
}

int vfs_count_children(vfs_node_t *dir)
{
    int count = 0;

    if(!dir)
        return 0;

    vfs_node_t *node =
        dir->child;

    while(node)
    {
        count++;
        node = node->next;
    }

    return count;
}

vfs_node_t *vfs_get_child(
    vfs_node_t *dir,
    int index)
{
    if(!dir || index < 0)
        return 0;

    vfs_node_t *node =
        dir->child;

    while(node && index--)
        node = node->next;

    return node;
}

vfs_node_t *vfs_find(
    vfs_node_t *dir,
    const char *name)
{
    if(!dir)
        return 0;

    vfs_node_t *node =
        dir->child;

    while(node)
    {
        int i = 0;

        while(node->name[i] &&
              name[i] &&
              node->name[i] == name[i])
            i++;

        if(node->name[i] == 0 &&
           name[i] == 0)
            return node;

        node = node->next;
    }

    return 0;
}

int vfs_read(vfs_node_t *node)
{
    if(!node || node->is_dir)
        return 0;

    if(!vfs_fs)
        return 1;

    unsigned int size = 0;

    if(!fat32_read_file(
            vfs_fs,
            node->first_cluster,
            (unsigned char *)node->data,
            VFS_NODE_DATA - 1,
            &size))
        return 0;

    node->size = size;
    node->data[size] = 0;

    return 1;
}

int vfs_write(vfs_node_t *node)
{
    if(!node || node->is_dir)
        return 0;

    if(!vfs_fs)
        return 1;

    if(node->size >= VFS_NODE_DATA)
        node->size = VFS_NODE_DATA - 1;

    if(!fat32_write_file(
            vfs_fs,
            &node->first_cluster,
            (const unsigned char *)node->data,
            node->size))
        return 0;

    return fat32_update_entry(
        vfs_fs,
        node->dir_entry_lba,
        node->dir_entry_offset,
        node->first_cluster,
        node->size,
        0,
        node->name);
}

int vfs_rename(
    vfs_node_t *node,
    const char *name)
{
    if(!node || !name)
        return 0;

    if(vfs_fs)
    {
        if(!fat32_update_entry(
                vfs_fs,
                node->dir_entry_lba,
                node->dir_entry_offset,
                node->first_cluster,
                node->size,
                node->is_dir,
                name))
            return 0;
    }

    copy_name(node->name, name);

    return 1;
}

int vfs_delete(vfs_node_t *node)
{
    if(!node || node == root)
        return 0;

    if(node->child)
        return 0;

    if(vfs_fs)
    {
        if(node->is_dir &&
           !fat32_directory_empty(
                vfs_fs,
                node->first_cluster))
            return 0;

        if(!fat32_delete_entry(
                vfs_fs,
                node->dir_entry_lba,
                node->dir_entry_offset,
                node->first_cluster))
            return 0;
    }

    if(node->parent)
    {
        vfs_node_t *cur =
            node->parent->child;

        vfs_node_t *prev = 0;

        while(cur && cur != node)
        {
            prev = cur;
            cur = cur->next;
        }

        if(cur == node)
        {
            if(prev)
                prev->next = cur->next;
            else
                node->parent->child =
                    cur->next;
        }
    }

    node->parent = 0;
    node->next = 0;

    return 1;
}

void vfs_init(fat32_fs_t *fs)
{
    vfs_used = 0;
    vfs_fs = fs;

    root = vfs_alloc();

    if(!root)
        return;

    copy_name(root->name, "/");
    root->is_dir = 1;
    root->first_cluster =
        fs ? fs->root_cluster : 0;

    if(fs && fs->mounted)
    {
        load_directory(root);

        if(vfs_count_children(root) == 0)
        {
            vfs_node_t *home =
                vfs_create_dir(root, "home");

            vfs_node_t *readme =
                vfs_create_file(
                    home,
                    "readme.txt");

            vfs_node_t *config =
                vfs_create_file(
                    home,
                    "config.sys");

            if(readme)
            {
                readme->data[0] = 'H';
                readme->data[1] = 'i';
                readme->size = 2;
                vfs_write(readme);
            }

            if(config)
            {
                config->size = 0;
                config->data[0] = 0;
                vfs_write(config);
            }
        }

        return;
    }

    vfs_node_t *home =
        vfs_create_dir(root, "home");

    vfs_node_t *readme =
        vfs_create_file(
            home,
            "readme.txt");

    vfs_create_file(
        home,
        "config.sys");

    if(readme)
    {
        readme->data[0] = 'H';
        readme->data[1] = 'i';
        readme->data[2] = 0;
        readme->size = 2;
    }
}
