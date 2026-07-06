#include "vfs.h"

static vfs_node_t vfs_pool[VFS_MAX_NODES];
static int vfs_used = 0;

static vfs_node_t* root = 0;

vfs_node_t* vfs_get_root()
{
    return root;
}

static vfs_node_t* vfs_alloc()
{
    if(vfs_used >= VFS_MAX_NODES)
        return 0;

    return &vfs_pool[vfs_used++];
}

vfs_node_t* vfs_create(vfs_node_t* parent, const char* name, int is_dir)
{
    vfs_node_t* n = vfs_alloc();
    if(!n) return 0;

    int i = 0;
    while(name[i] && i < 31)
    {
        n->name[i] = name[i];
        i++;
    }
    n->name[i] = 0;

    n->is_dir = is_dir;
    n->parent = parent;
    n->child = 0;
    n->next = 0;
    n->data[0] = 0;

    if(parent)
    {
        if(!parent->child)
            parent->child = n;
        else
        {
            vfs_node_t* c = parent->child;
            while(c->next) c = c->next;
            c->next = n;
        }
    }

    return n;
}

int vfs_count_children(vfs_node_t* dir)
{
    int c = 0;
    vfs_node_t* n = dir->child;

    while(n)
    {
        c++;
        n = n->next;
    }

    return c;
}

vfs_node_t* vfs_get_child(vfs_node_t* dir, int index)
{
    vfs_node_t* n = dir->child;

    while(n && index--)
        n = n->next;

    return n;
}

void vfs_init()
{
    vfs_used = 0;

    root = vfs_create(0, "/", 1);

    vfs_node_t* home = vfs_create(root, "home", 1);

    vfs_node_t* f1 = vfs_create(home, "readme.txt", 0);
    vfs_node_t* f2 = vfs_create(home, "config.sys", 0);

    f1->data[0] = 'H';
    f1->data[1] = 'i';
    f1->data[2] = 0;
}
