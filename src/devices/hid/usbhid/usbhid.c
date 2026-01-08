#include "usbhid.h"

#ifdef __KERNEL_SUPPORT_DEV_USB_DEVICE_HID

# include "lib/stdio.h"
# include "memory/kmalloc.h"

static bool usbhid_iface_driver_initialized            = 0;
static struct usb_interface_driver usbhid_iface_driver = { 0 };

static const uint8_t usbhid_set_protocol_boot[8] = { 0x21, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const uint8_t usbhid_set_protocol_report[8] = { 0x21, 0x0B, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };

int usbhid_set_protocol(usb_interface_t *iface, bool use_report) {
    if (!iface) {
        printf("ERROR: ushbid_set_protocol: iface is NULL!\n");
        return -EINVAL;
    }

    if (iface->iface_driver != &usbhid_iface_driver) {
        printf("ERROR: calling usbhid_set_protocol on a non-HID device!\n");
        return -EINVAL;
    }

    struct usbhid_interface_driver_data *driver_data = (struct usbhid_interface_driver_data *)iface->iface_driver_data;

    if (!driver_data->supports_boot) {
        if (use_report) {
            printf("WARN: calling usbhid_set_protocol to set the device to use the report protocol, but the device "
                   "only supports the report protocol, so we can't set protocol! Not sending anything.\n");
            return 0;
        } else {
            printf("ERROR: usbhid_set_protocol: the boot protocol is not supported by this device!\n");
            return -EINVAL;
        }
    }

    usb_control_transfer(iface->device, use_report ? usbhid_set_protocol_report : usbhid_set_protocol_boot, NULL, 0,
                         false, 0);
}

int usbhid_bind_driver(usb_interface_t *iface) {
    if (iface->i_class != USB_IFACE_CLASS_HID) {
        printf("ERROR: calling usbhid_bind_driver() on a non-HID interface.\n");
        return -EINVAL;
    }

    printf("usbhid_bind_driver: binding driver...\n");

    if (!usbhid_iface_driver_initialized) {
        /* TODO: populate driver */
    }

    iface->iface_driver_data = kmalloc_heap(sizeof(struct usbhid_interface_driver_data));
    if (!iface->iface_driver_data) {
        printf("OOM ERROR: usbhid_bind_driver: failed to allocate iface->iface_driver_data.\n");
        return -ENOMEM;
    }
    iface->iface_driver = &usbhid_iface_driver;

    struct usbhid_interface_driver_data *driver_data = (struct usbhid_interface_driver_data *)iface->iface_driver_data;

    switch (iface->i_protocol) {
    case 0x01: {
        driver_data->type = USBKBD;
        printf(" - device type: keyboard\n");
        break;
    }
    case 0x02: {
        driver_data->type = USBMOUSE;
        printf(" - device type: mouse\n");
        break;
    }
    default: {
        printf(" - device type unknown");
        break;
    }
    }

    driver_data->supports_boot = (iface->i_subclass == 1) ? true : false;
    if (driver_data->supports_boot) {
        printf(" - supports boot protocol\n");
    } else {
        printf(" - does not support boot protocol\n");
    }

    switch (driver_data->type) {
    case USBKBD: {
        printf(" - loading usbkbd driver...\n");
        usbkbd_driver_init(iface);
        break;
    }
    default: {
        printf(" - NO DRIVER!\n");
    }
    }

    return 0;
}

#endif // __KERNEL_SUPPORT_DEV_USB_DEVICE_HID