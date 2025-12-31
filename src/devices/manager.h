#pragma once

#include "plenjos/errno.h"

#include <stddef.h>
#include <stdint.h>

#define ERR_NO_DRIVER 1
#define ERR_NO_MEM    -ENOMEM

// TODO: make this a build option
// For now, we include all device drivers in the kernel build

#define __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD     1
#define __KERNEL_SUPPORT_DEV_INPUT_KEYBOARD_PS2 1
#define __KERNEL_SUPPORT_DEV_USB                1
#define __KERNEL_SUPPORT_DEV_USB_HOST_UHCI      1

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

int device_manager_init();