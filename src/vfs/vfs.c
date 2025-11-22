#include <stdint.h>

#include "memory/kmalloc.h"

#include "vfs/vfs.h"

void vfs_init() {
    vfs_root = (vfs_handle_t *)kmalloc_heap(sizeof(vfs_handle_t));

    vfs_root->type = VFS_NODE_TYPE_DIR;
}

vfs_handle_t *vfs_open(const char *path, const char *mode) {
    vfs_handle_t *f = kmalloc_heap(sizeof(vfs_handle_t));

    return f;
}

void vfs_close(vfs_handle_t *f) {

}

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len) {
    if (f) {
        return f->read(f, buf, len);
    }
    return -1;
}

ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    if (f) {
        return f->write(f, buf, len);
    }
    return -1;
}

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    return -1;
}