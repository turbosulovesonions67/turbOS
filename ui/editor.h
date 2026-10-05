#ifndef TURBOS_EDITOR_H
#define TURBOS_EDITOR_H

#include "fs/vfs.h"

void draw_editor(void);
void editor_open_file(vfs_node_t *file);
void editor_save(void);
void handle_editor_key(int key);

#endif
