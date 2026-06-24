#pragma once

#include "plenjos/errno.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ERR_NO_DRIVER 1
#define ERR_NO_MEM    -ENOMEM

typedef struct device device_t;

// TODO: make this a build option
// For now, we include all device drivers in the kernel build

#define __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD     1
#define __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD_PS2 1
#define __KERNEL_SUPPORT_DEV_USB                1
#define __KERNEL_SUPPORT_DEV_USB_HOST_UHCI      1
#define __KERNEL_SUPPORT_DEV_USB_DEVICE_HID     1

#define __KERNEL_SUPPORT_DEV_PCI         1
#define __KERNEL_SUPPORT_DEV_STORAGE_ATA 1
#define __KERNEL_SUPPORT_DEV_STORAGE_IDE 1

/*
section INPUT_DEVICES:
    option __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD:
        option __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD_PS2

        also requires __KERNEL_SUPPORT_DEV_USB_HID:
            option __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD_USB_HID

section PCI_DEVICES:
    option __KERNEL_SUPPORT_DEV_PCI:
        option __KERNEL_SUPPORT_DEV_PCIe

    option __KERNEL_SUPPORT_DEV_USB:
            option __KERNEL_SUPPORT_DEV_USB_HOST_UHCI
            option __KERNEL_SUPPORT_DEV_USB_HOST_OHCI
            option __KERNEL_SUPPORT_DEV_USB_HOST_EHCI
            option __KERNEL_SUPPORT_DEV_USB_HOST_XHCI

            option __KERNEL_SUPPORT_DEV_USB_DEVICE_HID
            option __KERNEL_SUPPORT_DEV_USB_DEVICE_MASS_STORAGE

section STORAGE_DEVICES:
    option __KERNEL_SUPPORT_DEV_STORAGE_ATA:
        option __KERNEL_SUPPORT_DEV_STORAGE_IDE
        option __KERNEL_SUPPORT_DEV_STORAGE_SATA_AHCI
*/

/**
 * Basic device structure:
 * 
 * Every device has exactly one physical path under /sys and as many virtual paths under /dev as it wants.
 * The physical path could be something like /sys/pci/0000:00
 * 
 * All base device types and flags are defined in headers, regardless of whether build support is enabled.
 */

typedef uint64_t device_flags_t;

typedef struct device_id {
    
} device_id_t;

struct device {
    device_t *parent;

    device_flags_t flags;
};

#define DEVICE_FLAG_BLOCK 0x01 // Bit 0: 0 = character device, 1 = block device

inline bool is_block_device(device_t *dev) {
    return (dev->flags & DEVICE_FLAG_BLOCK) != 0;
}

inline bool is_character_device(device_t *dev) {
    return (dev->flags & DEVICE_FLAG_BLOCK) == 0;
}

typedef enum {
    BUS_CLASS_PCI,
    BUS_CLASS_USB,
    BUS_CLASS_IDE,
    BUS_CLASS_PS2,
    BUS_CLASS_PLATFORM, // This class is for things like the RTC, PS/2 controller, etc.
    BUS_CLASS_SATA,
    BUS_CLASS_OTHER
} bus_base_class_t;

typedef enum {
    DEVICE_CLASS_CONTROLLER,
    DEVICE_CLASS_
} device_class_t;

int device_manager_init();