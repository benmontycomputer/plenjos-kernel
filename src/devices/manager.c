#include "manager.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/special_mem/fixed_size_allocator.h"

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

endpoint_t *register_device_endpoint(char *name) {
    endpoint_t *endpoint = fsa_alloc(&_endpoint_fsa);

    if (!endpoint) {
        kout(KERNEL_WARN, "register_device_endpoint(): fsa_alloc() failed (OOM?)\n");
        return NULL;
    }

    strncpy(endpoint->name, name, NAME_MAX);
    endpoint->name[NAME_MAX] = 0;

    
}

int device_manager_init() {
    vfs_mkdir("/dev", 0, 0, 0755);

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