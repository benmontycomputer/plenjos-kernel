#pragma once

#include "devices/manager.h"

#ifdef __KERNEL_SUPPORT_DEV_USB_DEVICE_HID

# include "devices/usb/usb.h"

# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>

typedef enum {
    USBKBD,
    USBMOUSE
} usbhid_device_type_t;

struct usbhid_interface_driver_data {
    usbhid_device_type_t type;
    bool supports_boot;

    uint8_t last_boot_status[8];
};

int usbhid_bind_driver(usb_interface_t *iface);
int usbhid_set_protocol(usb_interface_t *iface, bool use_report);
int usbkbd_driver_init(usb_interface_t *iface);

#endif // __KERNEL_SUPPORT_DEV_USB_DEVICE_HID