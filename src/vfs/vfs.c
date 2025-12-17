#include <stdint.h>

#include "memory/kmalloc.h"

#include "proc/proc.h"

#include "vfs/vfs.h"
#include "vfs/kernelfs.h"

#include "plenjos/syscall.h"

void vfs_init() {
    kernelfs_init();
}

void vfs_close(vfs_handle_t *f) {
    f->close(f);
}

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len) {
    if (f && f->read) {
        return f->read(f, buf, len);
    }
    return -1;
}

ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    if (f && f->write) {
        return f->write(f, buf, len);
    }
    return -1;
}

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    if (f && f->seek) {
        return f->seek(f, offset, whence);
    }
    return -1;
}

vfs_handle_t vfs_open(const char *path, syscall_open_flags_t flags, uid_t uid, gid_t gid, mode_t mode) {
    // First, find the fscache node
}