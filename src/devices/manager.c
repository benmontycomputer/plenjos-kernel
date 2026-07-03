#include "manager.h"

#include "lib/special_mem/fixed_size_allocator.h"
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

    struct kstat stat = { 0 };
    stat.mode         = S_DFLT | ((driver->flags & ENDPOINT_DRIVER_FLAG_BLOCK) ? S_IFBLK : S_IFCHR);
    stat.dev          = full_dev(ENDPOINT_MEM, ENDPOINT_MEM_DEVFS);
    stat.rdev         = endpoint->dev;

    int res = kernelfs_helper_create_file("/dev", (const char *)endpoint->name, stat, driver->read, driver->write,
                                          driver->seek, endpoint->func_args);
    if (res) {
        kout(KERNEL_WARN, "register_device_endpoint(): kernelfs_helper_create_file() failed on endpoint %s (rdev=%x)\n",
             endpoint->name, endpoint->dev);
        fsa_free(&_endpoint_fsa, endpoint);
        return NULL;
    }

    kout(KERNEL_INFO, "Registered endpoint %s (rdev=%x)\n", endpoint->name, endpoint->dev);

    return endpoint;
}

int device_manager_init() {
    vfs_mkdir("/dev", (struct kstat) { .mode = S_DFLT });

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