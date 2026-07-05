#include "manager.h"

#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/special_mem/flex_array_hashmap.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "plenjos/sysmacros.h"
#include "sysdrivers.h"
#include "vfs/fscache.h"
#include "vfs/kernelfs.h"

// TODO: IMPORTANT: this entire stack is not thread-safe (and neither is kernelfs)

// Keyboard support is built-in; I don't plan to make it a config option.
#include "devices/input/keyboard/keyboard.h"

#ifdef __KERNEL_SUPPORT_DEV_PCI
# include "devices/pci/pci.h"
#endif

#ifdef __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD_PS2
# include "devices/input/keyboard/drivers/ps2kbd.h"
#endif

#ifdef __KERNEL_SUPPORT_DEV_STORAGE_IDE
# include "devices/storage/ide.h"
#endif

#ifdef __KERNEL_SUPPORT_DEV_USB_HOST_UHCI
# include "devices/usb/host/uhci.h"
#endif

static fixed_size_allocator_t _endpoint_fsa = FSA_DEFAULT(sizeof(endpoint_t));
static flex_array_hashmap_t *_devfs_fah     = NULL;

#define DEVFS_ENDPOINT_MODE 0600

typedef struct devfs_cache_data {
    endpoint_t *endpoint;
    uint64_t unused[3];
} devfs_cache_data_t;

int devfs_root_load(fscache_node_t *node, const char *name, fscache_node_t *out);
ssize_t devfs_root_read(vfs_handle_t *handle, void *buf, size_t len);
off_t devfs_root_seek(vfs_handle_t *handle, off_t offset, vfs_seek_whence_t whence);

vfs_ops_block_t devfs_root_ops = {
    .close        = NULL,
    .create_child = NULL,
    .fsname       = "devfs",
    .load_node    = devfs_root_load,
    .read         = devfs_root_read,
    .write        = NULL,
    .seek         = NULL, // devfs_root_seek,
    .unload_node  = NULL,
};

ssize_t devfs_endpoint_read(vfs_handle_t *handle, void *buf, size_t len);
ssize_t devfs_endpoint_write(vfs_handle_t *handle, const void *buf, size_t len);
off_t devfs_endpoint_seek(vfs_handle_t *handle, off_t offset, vfs_seek_whence_t whence);

vfs_ops_block_t devfs_endpoint_ops = {
    .close        = NULL,
    .create_child = NULL,
    .fsname       = "devfs",
    .load_node    = NULL,
    .read         = devfs_endpoint_read,
    .write        = devfs_endpoint_write,
    .seek         = devfs_endpoint_seek,
    .unload_node  = NULL,
};

void mount_devfs() {
    fscache_node_t *node = NULL;
    int res              = fscache_request_node_for_mounting("/dev", 0, &node);
    if (res) {
        kout(KERNEL_SEVERE_FAULT,
             "mount_devfs(): fscache_request_node_for_mounting() failed (errno %d)! Can't mount devfs.\n", res);
    }

    node->fsops = &devfs_root_ops;

    _fscache_release_node_modifiable(node);
}

endpoint_t *register_device_endpoint(char *name, endpoint_driver_t *driver, uint64_t minor, device_t *hw_dev,
                                     void *func_args) {
    endpoint_t *endpoint = fsa_alloc(&_endpoint_fsa);

    if (!endpoint) {
        kout(KERNEL_WARN, "register_device_endpoint(): fsa_alloc() failed (OOM?)\n");
        return NULL;
    }

    strncpy(endpoint->name, name, NAME_MAX);
    endpoint->name[NAME_MAX] = 0;
    endpoint->dev            = full_dev(driver->major, minor);
    endpoint->driver         = driver;
    endpoint->func_args      = func_args;
    endpoint->hw_dev         = hw_dev;

    /* struct kstat stat = { 0 };
    stat.mode         = S_DFLT | ((driver->flags & ENDPOINT_DRIVER_FLAG_BLOCK) ? S_IFBLK : S_IFCHR);
    stat.dev          = full_dev(ENDPOINT_MEM, ENDPOINT_MEM_DEVFS);
    stat.rdev         = endpoint->dev; */

    /* int res = kernelfs_helper_create_file("/dev", (const char *)endpoint->name, stat, driver->read, driver->write,
                                          driver->seek, endpoint->func_args); */
    void *out_old = NULL;
    int res       = fah_insert(_devfs_fah, endpoint, &out_old);
    if (res) {
        kout(KERNEL_WARN, "register_device_endpoint(): kernelfs_helper_create_file() failed on endpoint %s (rdev=%x)\n",
             endpoint->name, endpoint->dev);
        fsa_free(&_endpoint_fsa, endpoint);
        return NULL;
    }
    if (out_old) {
        kout(KERNEL_SEVERE_FAULT, "register_device_endpoint(): endpoint %s already taken!\n", name);
    }

    kout(KERNEL_INFO, "Registered endpoint %s (rdev=%p)\n", endpoint->name, endpoint->dev);

    return endpoint;
}

const char *endpoint_key_func(void *data) {
    endpoint_t *endpoint = (endpoint_t *)data;
    return (const char *)endpoint->name;
}

int device_manager_init() {
    _devfs_fah = fah_new(endpoint_key_func, NULL);

    // TODO: make this better, we shouldn't use one global buffer as our entire keyboard layer.
    init_keyboard();

    /* PCI must come *before* PS/2, as the USB controllers must be initialized to disable any PS/2 emulation. */
    /* TODO: do we properly disable PS/2 emulation? */
#ifdef __KERNEL_SUPPORT_DEV_PCI
    pci_scan();
#endif

#ifdef __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD_PS2
    init_ps2_keyboard();
#endif

#ifdef __KERNEL_SUPPORT_DEV_STORAGE_IDE
    ide_init();
#endif

    return 0;
}

int devfs_root_load(fscache_node_t *node, const char *name, fscache_node_t *out) {
    if (!node || !name || !out) {
        return -EINVAL;
    }

    while (*name && *name == '/') {
        name++;
    }
    if (!*name) {
        kout(KERNEL_WARN, "devfs_root_load(): name was empty after removing leading slashes\n");
        return -EIO;
    }
    endpoint_t *endpoint = fah_get(_devfs_fah, name);
    if (!endpoint) {
        return -ENOENT;
    }
    devfs_cache_data_t *data = (devfs_cache_data_t *)out->internal_data;
    data->endpoint           = endpoint;

    struct kstat stat = {
        .dev  = DEV_DEVFS,
        .mode = DEVFS_ENDPOINT_MODE | is_block_endpoint(endpoint) ? S_IFBLK : S_IFCHR,
        .rdev = endpoint->dev,
    };

    fscache_node_populate(out, 0, name, stat, &devfs_endpoint_ops);
    return 0;
}

typedef struct devfs_root_read_each_func_args {
    void *buf;
    size_t prev_dirents;
    uint64_t *handle_pos_ref;
} devfs_root_read_each_func_args_t;

void devfs_root_read_each_func(void *item, void *func_args) {
    devfs_root_read_each_func_args_t *args = (devfs_root_read_each_func_args_t *)func_args;

    const char *key = (const char *)item;
    endpoint_t *endpoint = fah_get(_devfs_fah, key);

    if (!endpoint) {
        kout(KERNEL_SEVERE_FAULT, "devfs_root_read_each_func(): endpoint %s not found in fah\n", key);
        return;
    }

    struct plenjos_dirent dirent = {
        .type = is_block_endpoint(endpoint) ? DT_BLK : DT_CHR,
    };

    strncpy(dirent.d_name, key, sizeof(dirent.d_name) - 1);
    // Already NULL terminated because dirent is zeroed to start with

    memcpy((uint8_t *)args->buf + (args->prev_dirents * sizeof(struct plenjos_dirent)), (const void *)&dirent, sizeof(struct plenjos_dirent));

    args->prev_dirents++;
}

ssize_t devfs_root_read(vfs_handle_t *handle, void *buf, size_t len) {
    if (len % sizeof(struct plenjos_dirent) != 0) {
        printf("devfs_root_read(): length %zu is not a multiple of dirent size %zu!\n", len,
               sizeof(struct plenjos_dirent));
        return -EINVAL;
    }

    uint64_t pos = handle->instance_data[0];
    uint64_t cnt = len / sizeof(struct plenjos_dirent);

    devfs_root_read_each_func_args_t args = {
        .buf            = buf,
        .prev_dirents   = 0,
    };

    ssize_t res = fah_each(_devfs_fah, pos, cnt, devfs_root_read_each_func, &args);

    handle->instance_data[0] += args.prev_dirents;
    return args.prev_dirents * sizeof(struct plenjos_dirent);
}

off_t devfs_root_seek(vfs_handle_t *handle, off_t offset, vfs_seek_whence_t whence) {
    // TODO: implement
    return -ENOSYS;
}

ssize_t devfs_endpoint_read(vfs_handle_t *handle, void *buf, size_t len) {
    if (!handle || !handle->backing_node) {
        kout(KERNEL_WARN, "devfs_endpoint_read(): handle or backing node is NULL!\n");
        return -EINVAL;
    }

    devfs_cache_data_t *data = (devfs_cache_data_t *)handle->backing_node->internal_data;
    return data->endpoint->driver->read(handle, buf, len);
}

ssize_t devfs_endpoint_write(vfs_handle_t *handle, const void *buf, size_t len) {
    if (!handle || !handle->backing_node) {
        kout(KERNEL_WARN, "devfs_endpoint_read(): handle or backing node is NULL!\n");
        return -EINVAL;
    }

    devfs_cache_data_t *data = (devfs_cache_data_t *)handle->backing_node->internal_data;
    return data->endpoint->driver->write(handle, buf, len);
}

off_t devfs_endpoint_seek(vfs_handle_t *handle, off_t offset, vfs_seek_whence_t whence) {
    if (!handle || !handle->backing_node) {
        kout(KERNEL_WARN, "devfs_endpoint_read(): handle or backing node is NULL!\n");
        return -EINVAL;
    }

    devfs_cache_data_t *data = (devfs_cache_data_t *)handle->backing_node->internal_data;
    return data->endpoint->driver->seek(handle, offset, whence);
}