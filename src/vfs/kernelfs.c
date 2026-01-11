#include "vfs/kernelfs.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "plenjos/dirent.h"
#include "plenjos/errno.h"
#include "plenjos/stat.h"
#include "plenjos/syscall.h"
#include "proc/proc.h"

#include <stdint.h>

kernelfs_node_t *root_kernelfs_node = NULL;

// TODO: put these functions into a vfs_ops_block_t that can be shared among all kernelfs nodes, thus reducing memory
// usage
ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len);
off_t kernelfs_seek(vfs_handle_t *f, off_t offset, vfs_seek_whence_t whence);
int kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out);

ssize_t kernelfs_default_read_dir(vfs_handle_t *f, void *buf, size_t len);

int kernelfs_create_node(kernelfs_node_t *parent_node, const char *name, uint8_t type, uid_t uid, gid_t gid,
                         mode_t mode, kernelfs_open_func_t open, vfs_read_func_t read, vfs_write_func_t write,
                         vfs_seek_func_t seek, void *func_args, kernelfs_node_t **out);

typedef enum kernelfs_find_node_result {
    KERNELFS_FIND_NODE_HANDLE_ONLY     = 1,
    KERNELFS_FIND_NODE_SUCCESS         = 0,
    KERNELFS_FIND_NODE_PATH_NULL       = -1,
    KERNELFS_FIND_NODE_NOT_A_DIRECTORY = -2,
    KERNELFS_FIND_NODE_NOT_FOUND       = -3,
    KERNELFS_FIND_NODE_ALLOC_FAILED    = -4,
    KERNELFS_FIND_NODE_ROOT_NODE_NULL  = -5,
} kernelfs_find_node_result_t;

kernelfs_node_t *kernelfs_get_node_from_handle(vfs_handle_t *handle) {
    if (!handle || !handle->backing_node) {
        return NULL;
    }

    kernelfs_cache_data_t *cache_data = (kernelfs_cache_data_t *)handle->backing_node->internal_data;
    if (!cache_data) {
        return NULL;
    }

    return cache_data->node;
}

/* The following few functions are meant to only be put inside of a vfs_handle_t. Permissions MUST be checked
 * beforehand. */
ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (node && node->read) {
        return node->read(f, buf, len);
    }
    return -1;
}

ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (node && node->write) {
        return node->write(f, buf, len);
    }
    return -1;
}

off_t kernelfs_seek(vfs_handle_t *f, off_t offset, vfs_seek_whence_t whence) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (node && node->seek) {
        return node->seek(f, offset, whence);
    }
    return -1;
}

// This is a default function allowing for listing kernelfs directory contents.
ssize_t kernelfs_default_read_dir(vfs_handle_t *f, void *buf, size_t len) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (!node) {
        printf("kernelfs_default_read_dir: no kernelfs node on handle!\n");
        return -EIO;
    }

    if (node->type != DT_DIR) {
        printf("kernelfs_default_read_dir: kernelfs node %s is not a directory!\n", node->name);
        return -ENOTDIR;
    }

    if (len % sizeof(struct plenjos_dirent) != 0) {
        printf("kernelfs_default_read_dir: length %zu is not a multiple of dirent size %zu!\n", len,
               sizeof(struct plenjos_dirent));
        return -EINVAL;
    }

    kernelfs_handle_instance_data_t *instance_data = (kernelfs_handle_instance_data_t *)f->instance_data;
    // instance_data will never be NULL since it is actually a slight offset of the pointer to the handle itself; it
    // isn't malloc'd or anything.

    kernelfs_node_t *current_child = node->children;
    size_t total_bytes_copied      = 0;

    // Skip to the current position
    for (uint64_t i = 0; i < instance_data->pos; i++) {
        if (current_child) {
            current_child = current_child->next;
        } else {
            // No more children
            return 0;
        }
    }

    while (current_child) {
        struct plenjos_dirent dirent_entry;
        memset(&dirent_entry, 0, sizeof(struct plenjos_dirent));

        strncpy(dirent_entry.d_name, current_child->name, NAME_MAX + 1);
        dirent_entry.d_name[NAME_MAX] = '\0';
        dirent_entry.type             = current_child->type;

        if (total_bytes_copied + sizeof(struct plenjos_dirent) > len) {
            // No more space
            break;
        }

        memcpy((uint8_t *)buf + total_bytes_copied, &dirent_entry, sizeof(struct plenjos_dirent));
        total_bytes_copied += sizeof(struct plenjos_dirent);

        current_child = current_child->next;
    }

    instance_data->pos += (total_bytes_copied / sizeof(struct plenjos_dirent));

    return (ssize_t)total_bytes_copied;
}

// IMPORTANT: "out" must point to an already allocated fscache_node_t pointer
int kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out) {
    kernelfs_node_t *parent_node = node ? ((kernelfs_cache_data_t *)node->internal_data)->node : root_kernelfs_node;

    kernelfs_node_t *current_node;

    if (name == NULL) {
        current_node = parent_node;
        goto node_found;
    } else {
        current_node = parent_node->children;
    }

    while (current_node) {
        if (strncmp(current_node->name, name, NAME_MAX + 1) == 0) {
            goto node_found;
        }
        current_node = current_node->next;
    }

    printf("kernelfs_load: node %s not found under parent %s\n", name, parent_node ? parent_node->name : "(null)");
    return -ENOENT;

// Found the node
node_found:
    if (out) {
        // TODO: MEMORY LEAK: free this
        vfs_ops_block_t *fsops = kmalloc_heap(sizeof(vfs_ops_block_t));
        if (!fsops) {
            return -ENOMEM;
        }

        // Clear everything first; only assign to the pointers we actually want
        memset(fsops, 0, sizeof(vfs_ops_block_t));

        fsops->fsname = "kernelfs";
        fsops->read   = current_node->read;
        fsops->write  = current_node->write;
        fsops->seek   = current_node->seek;
        fsops->close  = NULL; // Use default vfs close
        fsops->create_child
            = (current_node->type == DT_DIR) ? kernelfs_create_child : NULL; // TODO: implement create_child
        fsops->load_node   = kernelfs_load;
        fsops->unload_node = NULL; // No internal data to clean up

        // TODO: implement size
        fscache_node_populate(out, current_node->type, 0, current_node->name, current_node->uid, current_node->gid,
                              current_node->mode, 0 /* current_node->size */, fsops);

        ((kernelfs_cache_data_t *)out->internal_data)->node = current_node;
    }

    return 0;
}

int kernelfs_create_child(fscache_node_t *parent, const char *name, dirent_type_t type, uid_t uid, gid_t gid,
                          mode_t mode, fscache_node_t *node) {
    if (!parent || !name || !node) {
        return -EINVAL;
    }

    if (parent->type != DT_DIR) {
        printf("kernelfs_create_child: parent node is not a directory!\n");
        return -ENOTDIR;
    }

    kernelfs_cache_data_t *parent_cache_data = (kernelfs_cache_data_t *)parent->internal_data;
    if (!parent_cache_data->node) {
        printf("kernelfs_create_child: no kernelfs node on the parent cache data!\n");
        return -EIO;
    }

    vfs_read_func_t read_func = NULL;
    // TODO: have a seek func as well

    if (type != DT_DIR) {
        printf("kernelfs_create_child: only creation of directory children are supported currently!\n");
        // TODO: is this the correct error code?
        // Also, do we want to eventually support creating regular files this way?
        return -ENOSYS;
    } else {
        read_func = kernelfs_default_read_dir;
    }

    kernelfs_node_t *new_node = NULL;
    ssize_t res = kernelfs_create_node(parent_cache_data->node, name, type, uid, gid, mode, NULL, read_func, NULL, NULL,
                                       NULL, &new_node);
    if (res < 0) {
        printf("kernelfs_create_child: failed to create kernelfs node %s under parent %s, errno %d\n", name,
               parent_cache_data->node->name, res);
        return res;
    }

    if (!parent->fsops || !parent->fsops->load_node) {
        printf("kernelfs_create_child: parent node has no load_node function!\n");
        return -EIO;
    }

    res = parent->fsops->load_node(parent, name, node);
    if (res < 0) {
        printf("kernelfs_create_child: failed to load newly created child %s under parent %s, errno %d\n", name,
               parent_cache_data->node->name, res);
        return res;
    }

    return res;
}

int kernelfs_helper_mkdir(const char *path, uid_t uid, gid_t gid, mode_t mode) {
    int res = 0;

    size_t path_copy_size = strlen(path) + 1;
    char *path_copy       = kmalloc_heap(path_copy_size);
    if (!path_copy) {
        return -ENOMEM;
    }

    strncpy(path_copy, path, path_copy_size);
    path_copy[path_copy_size - 1] = '\0';

    char *path_copy_ptr = path_copy;

    char *last_token = (char *)path_copy;
    while (*path_copy != '\0') {
        if (*path_copy == '/') {
            last_token = path_copy + 1;
        }
        path_copy++;
    }

    path_copy         = path_copy_ptr;
    *(last_token - 1) = '\0';

    vfs_handle_t *open_handle = NULL;
    // uid, etc. here simply specifies the user trying to find the parent directory; this function assumes full
    // privileges
    res = (int)vfs_open(path_copy, SYSCALL_OPEN_FLAG_DIRECTORY | SYSCALL_OPEN_FLAG_WRITE, 0, 0, &open_handle);
    if (res < 0) {
        printf("kernelfs_helper_mkdir: failed to open parent directory %s (trying to create %s), errno %d\n", path_copy,
               path, res);
        goto cleanup;
    } else {
        kernelfs_node_t *parent_node = kernelfs_get_node_from_handle(open_handle);
        if (!parent_node) {
            printf("kernelfs_helper_mkdir: failed to get kernelfs node from handle for parent directory %s (trying to "
                   "create %s)\n",
                   path_copy, path);
            res = -EIO;
            goto cleanup;
        }

        kernelfs_node_t *new_node = NULL;
        // TODO: implement some of these functions?
        res = kernelfs_create_node(parent_node, last_token, DT_DIR, uid, gid, mode, NULL, kernelfs_default_read_dir,
                                   NULL, NULL, NULL, &new_node);
        if (res < 0) {
            printf("kernelfs_helper_mkdir: failed to create kernelfs node %s for directory %s, errno %d\n", last_token,
                   path, res);
            goto cleanup;
        }
    }

cleanup:
    if (open_handle != NULL) vfs_close(open_handle);
    kfree_heap(path_copy);
    return res;
}

int kernelfs_helper_create_file(const char *parent, const char *name, uint8_t type, uid_t uid, gid_t gid, mode_t mode,
                                vfs_read_func_t read, vfs_write_func_t write, vfs_seek_func_t seek, void *func_args) {
    vfs_handle_t *parent_handle = NULL;
    ssize_t res = vfs_open(parent, SYSCALL_OPEN_FLAG_DIRECTORY | SYSCALL_OPEN_FLAG_WRITE, 0, 0, &parent_handle);
    if (res < 0) {
        printf("kernelfs_helper_create_file: failed to open parent directory %s, errno %d\n", parent, res);
        return res;
    }

    kernelfs_node_t *parent_node = kernelfs_get_node_from_handle(parent_handle);
    if (!parent_node) {
        printf("kernelfs_helper_create_file: failed to get kernelfs node from handle for parent directory %s\n",
               parent);
        vfs_close(parent_handle);
        return -EIO;
    }

    kernelfs_node_t *new_node = NULL;
    res = kernelfs_create_node(parent_node, name, type, uid, gid, mode, NULL, read, write, seek, func_args, &new_node);
    if (res < 0) {
        printf("kernelfs_helper_create_file: failed to create kernelfs node %s under parent %s, errno %d\n", name,
               parent, res);
        vfs_close(parent_handle);
        return res;
    }

    return 0;
}

int kernelfs_create_node(kernelfs_node_t *parent_node, const char *name, uint8_t type, uid_t uid, gid_t gid,
                         mode_t mode, kernelfs_open_func_t open, vfs_read_func_t read, vfs_write_func_t write,
                         vfs_seek_func_t seek, void *func_args, kernelfs_node_t **out) {
    if (!name || !parent_node || strlen(name) == 0) {
        printf("kernelfs_create_node: name or parent_node is NULL or empty!\n");
        return -EINVAL;
    }

    kernelfs_node_t *cur = parent_node->children;
    while (cur) {
        if (strncmp(cur->name, name, NAME_MAX + 1) == 0) {
            printf("kernelfs_create_node: %s already exists under parent %s!\n", name, parent_node->name);
            return -EEXIST;
        }
        cur = cur->next;
    }

    if (!parent_node) {
        printf("Parent node can't be null!\n");
        return -1;
    }

    if (parent_node->type != DT_DIR) {
        printf("Parent node is not a directory!\n");
        return -2;
    }

    kernelfs_node_t *new_node = (kernelfs_node_t *)kmalloc_heap(sizeof(kernelfs_node_t));

    if (!new_node) {
        printf("kernelfs_create_node: allocation failed for node %s.\n", name);
        return -3;
    }

    memset(new_node, 0, sizeof(kernelfs_node_t));

    strncpy(new_node->name, name, NAME_MAX);
    new_node->name[NAME_MAX] = '\0';
    new_node->type           = type;
    new_node->uid            = uid;
    new_node->gid            = gid;
    new_node->mode           = mode;
    new_node->open           = open;
    new_node->read           = read;
    new_node->write          = write;
    new_node->seek           = seek;

    new_node->prev     = NULL;
    new_node->next     = NULL;
    new_node->children = NULL;
    new_node->parent   = parent_node;

    new_node->func_args = func_args;

    // Insert into the parent node's children
    if (parent_node->children == NULL) {
        parent_node->children = new_node;
    } else {
        kernelfs_node_t *child = parent_node->children;
        while (child->next != NULL) {
            // printf("loopt");
            child = child->next;
        }
        child->next    = new_node;
        new_node->prev = child;
    }

    if (out) *out = new_node;

    return 0;
}

void kernelfs_init() {
    root_kernelfs_node = kmalloc_heap(sizeof(kernelfs_node_t));

    memset(root_kernelfs_node, 0, sizeof(kernelfs_node_t));

    root_kernelfs_node->type    = DT_DIR;
    root_kernelfs_node->name[0] = 0;

    root_kernelfs_node->uid  = 0;
    root_kernelfs_node->gid  = 0;
    root_kernelfs_node->mode = S_DFLT; // | S_IFDIR;

    root_kernelfs_node->read = kernelfs_default_read_dir;

    kernelfs_create_node(root_kernelfs_node, "testfs", DT_DIR, 0, 0, S_DFLT, NULL, NULL, NULL, NULL, NULL, NULL);

    // kernelfs_open("/testfs", "r");
    // kernelfs_open("/testfs/tmp", "r");
}