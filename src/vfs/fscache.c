#include "vfs/fscache.h"
#include "vfs/vfs.h"

size_t fscache_nodes_count = 0;

void fscache_init() {
    fscache_nodes_count = 0;

    sizeof(fscache_node_t);
}