#pragma once

#include "devices/manager.h"

#ifdef __KERNEL_SUPPORT_DEV_USB

# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>

/**
 * All USB device drivers should bind to usb_interface_t, NOT usb_device_t.
 */

typedef struct usb_device usb_device_t;
typedef struct usb_endpoint usb_endpoint_t;
typedef struct usb_interface usb_interface_t;

/* If bcd_USB <= 0x0200, the device can enumerate on USB 1.1. However, if bcd_USB >= 0x0300, it cannot. */
# define USB_DEVICE_DESCRIPTOR_BCD_USB_MAX_UHCI 0x0200
typedef struct usb_device_descriptor {
    uint8_t b_length;          /* 18 */
    uint8_t b_descriptor_type; /* 1 (device descriptor) */
    uint16_t bcd_USB;          /* USB version */
    uint8_t b_device_class;
    uint8_t b_device_subclass;
    uint8_t b_device_protocol;
    uint8_t b_max_packet_size_0; /* Max packet size for endpoint 0 (8, 16, 32, 64) */
    uint16_t id_vendor_le;
    uint16_t id_product_le;
    uint16_t bcd_device;
    uint8_t i_manufacturer;
    uint8_t i_product;
    uint8_t i_serial_number;
    uint8_t b_num_configurations; /* Usually 1 */
} __attribute__((packed)) usb_device_descriptor_t;

# define USB_DESC_TYPE_CONFIG    0x02
# define USB_DESC_TYPE_INTERFACE 0x04
# define USB_DESC_TYPE_ENDPOINT  0x05

/* This struct only holds the first 9 bytes; the rest is dynamic */
typedef struct usb_configuration_descriptor {
    uint8_t b_length;              /* 9 */
    uint8_t b_descriptor_type;     /* 0x02 */
    uint16_t w_total_length;       /* total length including all subordinate descriptors */
    uint8_t b_num_interfaces;      /* number of interfaces */
    uint8_t b_configuration_value; /* used to select configuration */
    uint8_t i_configuration;       /* string descriptor index */
    uint8_t bm_attributes;         /* attributes bitmap (self-powered, remote wakeup) */
    uint8_t b_max_power;           /* maximum power in 2 mA units */
    uint8_t dyn_data[];
} __attribute__((packed)) usb_configuration_descriptor_t;

# define USB_IFACE_CLASS_HID 0x03

struct usb_interface_descriptor {
    uint8_t b_length;
    uint8_t b_descriptor_type; /* 0x04 */
    uint8_t i_num;
    uint8_t alt_setting;
    uint8_t num_endpoints;
    uint8_t i_class;
    uint8_t i_subclass;
    uint8_t i_protocol;
} __attribute__((packed));

#define USB_ENDPOINT_ATTR_MASK_TYPE 0x03
typedef enum usb_endpoint_attr {
    USB_ENDPOINT_ATTR_CONTROL = 0x00,
    USB_ENDPOINT_ATTR_ISO     = 0x01,
    USB_ENDPOINT_ATTR_BULK    = 0x02,
    USB_ENDPOINT_ATTR_INT     = 0x03,
} usb_endpoint_attr_t;

# define USB_ENDPOINT_DIR_OUT 0x00
# define USB_ENDPOINT_DIR_IN  0x80

struct usb_endpoint {
    uint8_t address; /* bit 7 = direction (1 = in) */
    usb_endpoint_attr_t attributes : 8;
    uint16_t max_packet_size;
    uint8_t interval;

    usb_interface_t *interface;
};

struct usb_interface {
    uint8_t i_num;
    uint8_t alt_setting;
    uint8_t i_class;
    uint8_t i_subclass;
    uint8_t i_protocol;

    uint8_t num_endpoints;
    usb_endpoint_t *endpoints;

    usb_device_t *device;

    struct usb_interface_driver *iface_driver;
    void *iface_driver_data;
};

struct usb_bus {
    uint16_t id;

    void *data;
};

struct usb_device {
    usb_device_t *parent;

    uint8_t address;     /* Host-assigned address */
    struct usb_bus *bus; /* Controller/bus */
    uint8_t port;        /* Root hub port */

    usb_endpoint_t ep0_in;
    usb_endpoint_t ep0_out;

    uint8_t num_interfaces;
    usb_interface_t *interfaces;

    usb_device_descriptor_t device_desc;
    usb_configuration_descriptor_t *config_desc;

    bool connected;
    bool enumerated;

    struct usb_host_driver *host_driver;
    void *host_driver_data;
};

typedef int (*usb_host_driver_control_transfer_func_t)(usb_device_t *dev, uint8_t *setup_packet, uint8_t *data_buffer,
                                                       size_t data_length, bool data_in, uint32_t timeout_ms);
typedef int (*usb_host_driver_bulk_transfer_func_t)(usb_endpoint_t *endpoint, uint8_t *buffer, size_t length,
                                                    uint32_t timeout_ms);
typedef int (*usb_host_driver_interrupt_transfer_func_t)(usb_endpoint_t *endpoint, uint8_t *buffer, size_t length,
                                                         uint32_t timeout_ms);
typedef int (*usb_host_driver_iso_transfer_func_t)(usb_endpoint_t *endpoint, uint8_t *buffer, size_t length,
                                                   uint32_t timeout_ms);

struct usb_host_driver {
    usb_host_driver_control_transfer_func_t control_transfer_func;
    usb_host_driver_bulk_transfer_func_t bulk_transfer_func;
    usb_host_driver_interrupt_transfer_func_t interrupt_transfer_func;
    usb_host_driver_iso_transfer_func_t iso_transfer_func;
};

struct usb_interface_driver {};

struct usb_setup_packet {
    uint8_t bm_request_type;
    uint8_t b_request;
    uint16_t w_value;
    uint16_t w_index;
    uint16_t w_length;
};

/**
 * All of these transfer functions will perform NULL checks on the endpoint/device.
 *
 * For all synchronous functions, if timeout_ms = 0, a reasonable default (determined by the host driver) will be used.
 */
int usb_control_transfer(usb_device_t *dev,
                         uint8_t *setup_packet /* 8 bytes; physaddr must be sub-4G for some controllers */,
                         uint8_t *data_buffer, size_t data_length, bool data_in, uint32_t timeout_ms);

/* Data transfers; these will validate endpoint type before passing it off to the host driver */
int usb_bulk_transfer(usb_endpoint_t *ep, uint8_t *buffer, size_t length, uint32_t timeout_ms);
int usb_interrupt_transfer(usb_endpoint_t *ep, uint8_t *buffer, size_t length, uint32_t timeout_ms);
int usb_iso_transfer(usb_endpoint_t *ep, uint8_t *buffer, size_t length, uint32_t timeout_ms);

struct usb_host_driver usb_populate_host_driver(usb_host_driver_control_transfer_func_t ctrl_transfer, usb_host_driver_bulk_transfer_func_t bulk_transfer);

usb_device_t *usb_setup_device(usb_device_t *parent, struct usb_bus *bus, uint8_t new_address, uint8_t port,
                               struct usb_host_driver *host_driver, void *host_driver_data);

#endif // __KERNEL_SUPPORT_DEV_USB