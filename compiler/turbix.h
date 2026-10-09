#ifndef TURBIX_COMPILER_H
#define TURBIX_COMPILER_H

#include "../fs/vfs.h"

int turbix_compile(vfs_node_t *source, vfs_node_t *output);

#endif
