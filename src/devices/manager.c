#include "manager.h"

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

int device_manager_init() {
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