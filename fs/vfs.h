#ifndef FS_H
#define FS_H

#define VFS_MAX_CHILDREN 16
#define VFS_MAX_NODES 64

typedef struct vfs_node
{
    char name[32];
    int is_dir;

    char data[1024];

    struct vfs_node* parent;
    struct vfs_node* child;
    struct vfs_node* next;

} vfs_node_t;

void vfs_init(void);

vfs_node_t* vfs_create(vfs_node_t* parent, const char* name, int is_dir);

vfs_node_t* vfs_create_file(vfs_node_t* parent, const char* name);
vfs_node_t* vfs_create_dir(vfs_node_t* parent, const char* name);

vfs_node_t* vfs_get_root(void);

int vfs_count_children(vfs_node_t* dir);
vfs_node_t* vfs_get_child(vfs_node_t* dir, int index);

vfs_node_t* vfs_find(vfs_node_t* dir, const char* name);

int vfs_delete(vfs_node_t* node);
int vfs_rename(vfs_node_t* node, const char* name);

#endif
