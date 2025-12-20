#pragma once

// TODO: implement locks or something to keep track of what files are opened by who

#include "kernel.h"
#include "lib/mode.h"
#include "plenjos/dirent.h"
#include "plenjos/stat.h"
#include "plenjos/syscall.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct fscache_node fscache_node_t;
typedef struct vfs_handle vfs_handle_t;
typedef struct vfs_ops_block vfs_ops_block_t;

typedef enum vfs_seek_whence {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2,
} vfs_seek_whence_t;

// TODO: across various functions, there is reused code that finds the last component of a path. Make this a helper
// function.

// User-invocable functions:

// The read function reads from almost all file types, but it doubles as getdents for directories
typedef ssize_t (*vfs_read_func_t)(vfs_handle_t *handle, void *buf, size_t len);

// The write function writes to a file
typedef ssize_t (*vfs_write_func_t)(vfs_handle_t *handle, const void *buf, size_t len);

// The seek function changes the current position in the file or directory
typedef ssize_t (*vfs_seek_func_t)(vfs_handle_t *handle, ssize_t offset, vfs_seek_whence_t whence);

// The close function closes the file handle
typedef int (*vfs_close_func_t)(vfs_handle_t *handle);

// fscache functions:

// Create child; this can be used to create files or directories (but NOT links). The resulting node is read-locked
// unless NULL is passed for out. Only the lower 12 bits of mode are honored (the rwxrwxrwx and special bits). If a node
// is passed in, the newly created child is loaded into the fscache (the node MUST be allocated already). Otherwise, the
// new child isn't loaded.
typedef ssize_t (*vfs_create_child_func_t)(fscache_node_t *parent, const char *name, dirent_type_t type, uid_t uid,
                                           gid_t gid, mode_t mode, fscache_node_t *node);

// The load function loads a child from a parent (directory)
typedef ssize_t (*vfs_load_func_t)(fscache_node_t *node, const char *name, fscache_node_t *out);

// The stat function; doesn't require any permissions on the node, but all parent directories must be executable to
// reach it This may be needed again if we implement any filesystems where stat can be changed without the knowledge of
// the fscache_node_t typedef ssize_t (*vfs_stat_func_t)(fscache_node_t *node, struct kstat *out_stat);

// The access remains unchanged throughout the lifetime of the handle; it is only checked on file open.
struct vfs_handle {
    access_t access;
    fscache_node_t *backing_node;

    // This is filesystem-specific
    uint64_t instance_data[4];
};

struct vfs_ops_block {
    char *fsname;

    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_seek_func_t seek;
    vfs_close_func_t close;

    // fscache functions
    vfs_create_child_func_t create_child;
    vfs_load_func_t load_node;
};

typedef struct vfs_filesystem {
    const char *name;
    bool (*mount)(const char *device, vfs_handle_t *mount_point);
} vfs_filesystem_t;

void vfs_init();

int vfs_close(vfs_handle_t *f);

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len);

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);

/**
 * If we create, only the lower 12 bits of mode_if_create (3 special bits + rwxrwxrwx) are honored.
 * The special bits are setuid, setgid, and sticky.
 * This function can't create directories; that must be done through a separate function.
 * If out is NULL, we just check for existence and permissions.
 *
 * Permissions are ONLY checked upon file opening; the handle's permissions don't change throughout its lifetime.
 * If this function creates a file, the resulting handle has whatever permissions are requested in flags (READ, WRITE,
 * EX), NOT LIMITED BY mode_if_create. mode_if_create is also masked by the process's umask before being applied.
 *
 * This function only works with absolute paths.
 */
// TODO: check process's umask instead of just assuming 022
ssize_t vfs_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create, uid_t uid, vfs_handle_t **out);

// Creates a regular file. Only the lower 12 bits of mode are honored (the rwxrwxrwx and special bits).
// Unlike other functions, this actually handles allocation of the fscache_node_t.
// If out isn't passed as NULL and the function succeeds, the resulting node is read-locked.
ssize_t vfs_creatat(fscache_node_t *parent, const char *name, uid_t uid, mode_t mode, fscache_node_t **out);

// Absolute path only
ssize_t vfs_mkdir(const char *path, uid_t uid, mode_t mode);

ssize_t vfs_mkdirat(vfs_handle_t *parent_handle, const char *name, uid_t uid, mode_t mode);