#include "pci.h"

#ifdef __KERNEL_SUPPORT_DEV_PCI

# include "lib/stdio.h"
# include "memory/kmalloc.h"

# ifdef __KERNEL_SUPPORT_DEV_USB
#  ifdef __KERNEL_SUPPORT_DEV_USB_HOST_UHCI
#   include "devices/usb/host/uhci.h"
#  endif
# endif

int pci_scan_process_serial_bus_controller(pci_device_t *dev) {
    switch (dev->subclass_code) {

    /* USB controller */
    case PCI_SUBCLASS_SERIAL_BUS_CONTROLLER_USB: {
# ifdef __KERNEL_SUPPORT_DEV_USB
        switch (dev->prog_if) {

        /* UHCI Host Controller */
        case PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_UHCI: {
#  ifdef __KERNEL_SUPPORT_DEV_USB_HOST_UHCI
            printf("PCI: Initializing UHCI USB controller (ProgIF %x)...\n", dev->prog_if);
            uhci_controller_t *uhci_controller = (uhci_controller_t *)kmalloc_heap(sizeof(uhci_controller_t));
            if (!uhci_controller) {
                printf("PCI: ERROR: Failed to allocate memory for UHCI controller structure.\n");
                return ERR_NO_MEM;
            }

            int res = uhci_init(uhci_controller, dev);
            if (res != 0) {
                printf("PCI: ERROR: Failed to initialize UHCI controller (res %d).\n", res);
                kfree_heap(uhci_controller);
                return ERR_NO_DRIVER;
            }

            printf("PCI: UHCI USB controller initialized successfully.\n");
            return 0;
#  else
            printf(
                "PCI: NO DRIVER (USB_HOST_UHCI): USB Serial Bus Controller detected (ProgIF %x), but UHCI support is "
                "not enabled.\n",
                dev->prog_if);
#  endif // __KERNEL_SUPPORT_DEV_USB_HOST_UHCI
            break;
        }

        /* Unknown */
        default: {
            printf("PCI: NO DRIVER (USB_DEV): USB Serial Bus Controller detected (ProgIF %x), but no driver is "
                   "available.\n",
                   dev->prog_if);
        }
        }
# else  // __KERNEL_SUPPORT_DEV_USB
        printf("PCI: NO DRIVER (USB_DEV): USB Serial Bus Controller detected (ProgIF %x), but USB support is not "
               "enabled.\n",
               dev->prog_if);
# endif // __KERNEL_SUPPORT_DEV_USB

        break;
    }
    default: {
        printf("PCI: NO DRIVER (SERIAL_BUS): Unknown Serial Bus Controller detected (Subclass %x, ProgIF %x).\n",
               dev->subclass_code, dev->prog_if);
        break;
    }
    }

    return ERR_NO_DRIVER;
}

#endif // __KERNEL_SUPPORT_DEV_PCI