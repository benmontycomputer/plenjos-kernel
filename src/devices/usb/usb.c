#include "usb.h"

#ifdef __KERNEL_SUPPORT_DEV_USB

# include "kernel.h"
# include "lib/stdio.h"
# include "memory/kmalloc.h"
# include "plenjos/errno.h"

static const uint8_t setup_packet_get_device_descriptor[8] = {
    0x80,       // bm_request_type: IN, standard, device
    0x06,       // b_request: GET_DESCRIPTOR
    0x00, 0x01, // w_value: 0x0100 (DEVICE_DESCRIPTOR, index 0)
    0x00, 0x00, // w_index: 0
    0x12, 0x00  // w_length: 18 bytes (device descriptor length)
};

static const uint8_t setup_packet_set_address[8] = {
    0x00, // bm_request_type: OUT, standard, device
    0x05, // b_request: SET_ADDRESS
    0x00 /* replace with address */,
    0x00, // w_value: new device address
    0x00,
    0x00, // w_index: 0
    0x00,
    0x00 // w_length: 0
};

/* First, read 9 bytes to get length. Then, modify the last 2 bytes to be the full length. */
static const uint8_t setup_packet_get_config[8] = {
    0x80,       // bmRequestType: IN, standard, device
    0x06,       // bRequest: GET_DESCRIPTOR
    0x00, 0x02, // wValue: 0x0200 → CONFIGURATION_DESCRIPTOR, index 0
    0x00, 0x00, // wIndex: 0
    0x09, 0x00  // wLength: 9 bytes first
};

/*
uint8_t setup_packet_get_hid[8] = {
    0x81,       // IN, standard, interface
    0x06,       // GET_DESCRIPTOR
    0x2200,     // wValue: 0x2200 → HID descriptor, index 0
    interface_num, 0x00, // wIndex: interface number
    sizeof(hid_desc),0x00 // wLength
};

uint8_t setup_packet_get_report[8] = {
    0x81,       // IN, class, interface
    0x06,       // GET_DESCRIPTOR
    0x2200,     // wValue: 0x2200 = report descriptor
    interface_num, 0x00, // wIndex: interface number
    report_length,0x00 // wLength = length from HID descriptor
};
*/

void usb_generate_setup_packet_set_address(uint8_t *out, uint8_t new_address) {
    memcpy(out, setup_packet_set_address, 8);
    out[2] = new_address;
}

void usb_generate_setup_packet_get_config_full(uint8_t *out, uint16_t len) {
    memcpy(out, setup_packet_get_config, 8);
    out[6] = len & 0xFF;
    out[7] = (len >> 8) & 0xFF;
}

int usb_control_transfer(usb_device_t *dev,
                         uint8_t *setup_packet /* 8 bytes; physaddr must be sub-4G for some controllers */,
                         uint8_t *data_buffer, uint16_t data_length, bool data_in) {
    if (dev->host_driver->control_transfer_func) {
        return dev->host_driver->control_transfer_func(dev, setup_packet, data_buffer, data_length, data_in);
    } else {
        printf("ERROR: usb_control_transfer called but the driver doesn't support it!\n");
        return -EIO;
    }
}

struct usb_host_driver usb_populate_host_driver(usb_host_driver_control_transfer_func_t ctrl_transfer) {
    return (struct usb_host_driver) { .control_transfer_func = ctrl_transfer };
}

usb_endpoint_t usb_generate_endpoint(uint8_t addr, bool in, usb_endpoint_attr_t attrs, usb_interface_t *interface,
                                     uint16_t max_packet_size) {
    usb_endpoint_t ep = { 0 };

    ep.address = addr & 0x7F;
    if (addr & 0x80) {
        printf("WARN: usb_generate_endpoint: bit 7 of endpoint address set.\n");
    }
    if (in) ep.address |= USB_ENDPOINT_DIR_IN;

    ep.attributes      = attrs;
    ep.interface       = interface;
    ep.max_packet_size = max_packet_size;

    return ep;
}

usb_device_t *usb_setup_device(usb_device_t *parent, struct usb_bus *bus, uint8_t new_address, uint8_t port,
                               struct usb_host_driver *host_driver, void *host_driver_data) {
    /* This function assumes the given port is populated */
    usb_device_t *dev = kmalloc_heap(sizeof(usb_device_t));
    memset(dev, 0, sizeof(usb_device_t));

    dev->address = 0; /* The real address will be set later */
    dev->bus     = bus;
    dev->port    = port;
    dev->parent  = parent;

    dev->ep0_out = usb_generate_endpoint(0, false, USB_ENDPOINT_ATTR_CONTROL, NULL, 8);
    dev->ep0_in  = usb_generate_endpoint(0, true, USB_ENDPOINT_ATTR_CONTROL, NULL, 8);

    dev->host_driver      = host_driver;
    dev->host_driver_data = host_driver_data;

    /* Get device descriptor */
    uint8_t desc_buf[18];

    int res = usb_control_transfer(dev, setup_packet_get_device_descriptor, desc_buf, 18, true);

    if (res == 0) {
        dev->device_desc = *(struct usb_device_descriptor *)desc_buf;
        /* printf("usb_setup_device: device descriptor for device on bus %d port %d:\n", bus.id, port);
        printf("  - USB version: %.4x\n", dev->device_desc.bcd_USB);
        printf("  - Vendor ID: %.4x\n", dev->device_desc.id_vendor_le);
        printf("  - Product ID: %.4x\n", dev->device_desc.id_product_le);
        printf("  - Device class: %.2x\n", dev->device_desc.b_device_class);
        printf("  - Device subclass: %.2x\n", dev->device_desc.b_device_subclass);
        printf("  - Device protocol: %.2x\n", dev->device_desc.b_device_protocol);
        printf("  - Max packet size (ep0): %d\n", dev->device_desc.b_max_packet_size_0);
        printf("  - Number of configurations: %d\n", dev->device_desc.b_num_configurations); */
    } else {
        printf("ERROR: usb_setup_device: get device descriptor failed.\n");
        goto err_res;
    }

    /* Set device address */
    uint8_t set_addr_packet[8];
    usb_generate_setup_packet_set_address(set_addr_packet, new_address);

    res = usb_control_transfer(dev, set_addr_packet, NULL, 0, false);

    if (res == 0) {
        printf("usb_setup_device: set device address to %d\n", (int)new_address);
        dev->address = new_address;
    } else {
        printf("ERROR: usb_setup_device: failed to set device address.\n");
        goto err_res;
    }

    /* Re-read device descriptor (sanity check) */
    res = usb_control_transfer(dev, setup_packet_get_device_descriptor, desc_buf, 18, true);

    if (res == 0) {
        dev->device_desc = *(struct usb_device_descriptor *)desc_buf;
        printf("usb_setup_device: device descriptor for device on bus %d port %d address %d:\n", dev->bus->id,
               dev->port, dev->address);
        printf("  - USB version: %.4x\n", dev->device_desc.bcd_USB);
        printf("  - Vendor ID: %.4x\n", dev->device_desc.id_vendor_le);
        printf("  - Product ID: %.4x\n", dev->device_desc.id_product_le);
        printf("  - Device class: %.2x\n", dev->device_desc.b_device_class);
        printf("  - Device subclass: %.2x\n", dev->device_desc.b_device_subclass);
        printf("  - Device protocol: %.2x\n", dev->device_desc.b_device_protocol);
        printf("  - Max packet size (ep0): %d\n", dev->device_desc.b_max_packet_size_0);
        printf("  - Number of configurations: %d\n", dev->device_desc.b_num_configurations);
    } else {
        printf("ERROR: usb_setup_device: re-get device descriptor failed.\n");
        goto err_res;
    }

    dev->ep0_in.max_packet_size  = dev->device_desc.b_max_packet_size_0;
    dev->ep0_out.max_packet_size = dev->device_desc.b_max_packet_size_0;

    /* To read the configuration header, we must first read the first 9 bytes to get its length, then read the whole
     * thing. */
    uint8_t cfg_header_buf_small[9];

    res = usb_control_transfer(dev, setup_packet_get_config, cfg_header_buf_small, 9, true);
    if (res != 0) {
        printf("ERROR: usb_setup_device: first get of config header failed.\n");
        goto err_res;
    }

    uint16_t cfg_header_len = cfg_header_buf_small[2] + (cfg_header_buf_small[3] << 8);

    dev->config_desc = kmalloc_heap(cfg_header_len);
    if (!dev->config_desc) {
        printf("OOM ERROR: usb_setup_device: couldn't allocate space for dev->config_desc.\n");
        goto err_res;
    }

    uint8_t cfg_header_full_packet[8];
    usb_generate_setup_packet_get_config_full(cfg_header_full_packet, cfg_header_len);

    res = usb_control_transfer(dev, cfg_header_full_packet, (uint8_t *)dev->config_desc, cfg_header_len, true);
    if (res != 0) {
        printf("ERROR: usb_setup_device: full get of config header failed (size %d).\n", cfg_header_len);
        goto err_res;
    }

    if (cfg_header_len != dev->config_desc->w_total_length) {
        printf("ERROR: usb_setup_device: initial read value of cfg_header_len doesn't match value on second read.\n");
        goto err_res;
    }

    /* Map interfaces */
    dev->num_interfaces = dev->config_desc->b_num_interfaces;
    dev->interfaces     = kmalloc_heap(dev->num_interfaces * sizeof(usb_interface_t));
    memset(dev->interfaces, 0, dev->num_interfaces * sizeof(usb_interface_t));

    uint8_t *desc     = (uint8_t *)((uint64_t)dev->config_desc + dev->config_desc->b_length);
    uint8_t *desc_end = (uint8_t *)((uint64_t)dev->config_desc + dev->config_desc->w_total_length);
    for (uint8_t i = 0; i < dev->num_interfaces; i++) {
        struct usb_interface_descriptor *iface_desc = (struct usb_interface_descriptor *)desc;
        usb_interface_t *iface                      = &dev->interfaces[i];

        iface->i_num         = iface_desc->i_num;
        iface->alt_setting   = iface_desc->alt_setting;
        iface->num_endpoints = iface_desc->num_endpoints;
        iface->i_class       = iface_desc->i_class;
        iface->i_subclass    = iface_desc->i_subclass;
        iface->i_protocol    = iface_desc->i_protocol;

        iface->device = dev;

        /* TODO: populate interface */
        printf("uhci: found interface %d\n", i);
        printf(" - class: %.2x\n", iface->i_class);
        printf(" - subclass: %.2x\n", iface->i_subclass);
        printf(" - protocol: %.2x\n", iface->i_protocol);

        desc += iface_desc->b_length;
        while (desc[1] != USB_DESC_TYPE_INTERFACE && desc < desc_end) {
            if (desc[1] == USB_DESC_TYPE_ENDPOINT) {
                /* TODO: populate endpoint */
                printf(" - uhci: found endpoint under interface %d\n", i);
            } else {
                /* TODO: handle unknown desc type */
                printf(" - uhci: found unknown descriptor under interface %d\n", i);
            }

            desc += desc[0];
        }
    }

    return dev;

err_res:
    if (dev && dev->config_desc) kfree_heap(dev->config_desc);
    if (dev) kfree_heap(dev);
    return NULL;
}

#endif // __KERNEL_SUPPORT_DEV_USB