/**
 * @file uhci.h
 * @brief UHCI USB Host Controller Support Header
 *
 * Header file for UHCI USB host controller support. Requires USB and PCI support.
 *
 * @pre USB support
 * @pre PCI support
 */

#pragma once

#include "devices/manager.h"

#ifdef __KERNEL_SUPPORT_DEV_USB_HOST_UHCI

# ifndef __KERNEL_SUPPORT_DEV_USB
#  error "UHCI support requires USB support to be enabled"
# else
#  include "devices/usb/usb.h"
# endif

# ifndef __KERNEL_SUPPORT_DEV_PCI
#  error "UHCI support requires PCI support to be enabled"
# else
#  include "devices/pci/pci.h"
# endif

# include "kernel.h"
# include "lib/lock.h"

# include <stddef.h>
# include <stdint.h>

/* Recommended: 100ms per TD, but 500ms total for a control transfer */
/* TODO: implement some sort of adaptive timeout system (slow devices might need more time than this) */
# define UHCI_DEFAULT_CONTROL_TIMEOUT 500U /* ms */
# define UHCI_DEFAULT_BULK_TIMEOUT_PER_PACKET 100U

// https://wiki.osdev.org/Universal_Host_Controller_Interface

/**
 * UHCI controllers are one of the two types of USB 1.1 host controllers (the other being OHCI). UHCI is an
 * Intel-developed standard that exists solely on the PCI bus (not even compatible with PCIe). UHCI controllers make
 * heavy use of I/O ports, as opposed to OHCI which uses memory-mapped I/O.
 */

/**
 * I/O registers:
 *
 * The offsets are relative to the controller's base address.
 *
 * - Offset - Name - - - Description - - - - - - Length - - *
 *   0x00:	  USBCMD	 Usb Command			 2 bytes	*
 *   0x02:	  USBSTS	 Usb Status				 2 bytes	*
 *   0x04:	  USBINTR	 Usb Interrupt Enable	 2 bytes	*
 *   0x06:	  FRNUM		 Frame Number			 2 bytes	*
 *   0x08:	  FRBASEADD	 Frame List Base Address 4 bytes	*
 *   0x0C:	  SOFMOD	 Start Of Frame Modify	 1 byte		*
 *   0x10:	  PORTSC1	 Port 1 Status/Control	 2 bytes	*
 *   0x12:	  PORTSC2	 Port 2 Status/Control	 2 bytes	*
 */

# define UHCI_IO_USBCMD    0x00 /* See UHCI_CMDREG_* */
# define UHCI_IO_USBSTS    0x02 /* See UHCI_STSREG_* */
# define UHCI_IO_USBINTR   0x04 /* See UHCI_INTRREG_* */
# define UHCI_IO_FRNUM     0x06 /* Number of processed entry of frame list */
# define UHCI_IO_FRBASEADD 0x08 /* 32-bit physical address of frame list; 4k aligned; 1024 entries */
# define UHCI_IO_SOFMOD    0x0C /* This port sets timing of frame. Value should be 0x40 */
# define UHCI_IO_PORTSC1   0x10 /* See UHCI_PORTSCREG_ */
# define UHCI_IO_PORTSC2   0x12 /* See UHCI_PORTSCREG_ */

/**
 * Command register:
 *  - Bits 15-8: reserved
 *  - Bit 7: max packet size (0 = 32 bytes, 1 = 64 bytes)
 *  - Bit 6: configure flag
 *  - Bit 5: software debug
 *  - Bit 4: global resume
 *  - Bit 3: global suspend
 *  - Bit 2: global reset
 *  - Bit 1: host controller reset (clears after reset is done)
 *  - Bit 0: run (1 = controller execute frame list entries)
 */
# define UHCI_CMDREG_MAX_PACKET_SIZE 0x80
# define UHCI_CMDREG_CONFIGURE_FLAG  0x40
# define UHCI_CMDREG_SOFTWARE_DEBUG  0x20
# define UHCI_CMDREG_GLOBAL_RESUME   0x10
# define UHCI_CMDREG_GLOBAL_SUSPEND  0x08
# define UHCI_CMDREG_GLOBAL_RESET    0x04
# define UHCI_CMDREG_HC_RESET        0x02
# define UHCI_CMDREG_RUN             0x01

/**
 * Status register: these bits can be cleared by writing a 1 to them
 *
 * Bits
 *  - Bits 15-6: reserved
 *  - Bit 5: halted (indicates status of frame list execution)
 *  - Bit 4: process error
 *  - Bit 3: system error
 *  - Bit 2: resume detected
 *  - Bit 1: error interrupt
 *  - Bit 0: interrupt
 */
# define UHCI_STSREG_HALTED 0x20
# define UHCI_STSREG_PERROR 0x10
# define UHCI_STSREG_SERROR 0x08
# define UHCI_STSREG_RESUME 0x04
# define UHCI_STSREG_ERRINT 0x02
# define UHCI_STSREG_INT    0x01

/**
 * Interrupt enable register:
 *
 * Bits
 *  - Bits 15-4: reserved
 *  - Bit 3: short packet (interrupt when short packet transferred)
 *  - Bit 2: complete transfer (interrupt after transfer of packet with IOC)
 *  - Bit 1: resume detect (interrupt when resume detected)
 *  - Bit 0: timeout CRC (interrupt on timeout or CRC error)
 */
# define UHCI_INTRREG_SHORTPACKET 0x08
# define UHCI_INTRREG_COMPLETE    0x04
# define UHCI_INTRREG_RESUME      0x02
# define UHCI_INTRREG_TIMEOUTCRC  0x01

/**
 * Port status/control register:
 *
 * Bits:
 *  - Bits 15-13: reserved
 *  - Bit 12: suspend
 *  - Bits 11-10: reserved
 *  - Bit 9: reset
 *  - Bit 8: low speed device attached
 *  - Bit 7: reserved; always set; used to detect number of ports
 *  - Bit 6: resume detected
 *  - Bits 5-4: line status
 *  - Bit 3: port enable changed (write-clear)
 *  - Bit 2: port enable
 *  - Bit 1: connection status changed (write-clear)
 *  - Bit 0: connection status
 */
# define UHCI_PORTSCREG_SUSPEND             0x1000
# define UHCI_PORTSCREG_RESET               0x0200
# define UHCI_PORTSCREG_LOWSPEED            0x0100
# define UHCI_PORTSCREG_RESUME_DETECTED     0x0040
# define UHCI_PORTSCREG_LINE_STATUS_MASK    0x0030
# define UHCI_PORTSCREG_PORT_ENABLE_CHANGED 0x0008
# define UHCI_PORTSCREG_PORT_ENABLE         0x0004
# define UHCI_PORTSCREG_CONN_STATUS_CHANGED 0x0002
# define UHCI_PORTSCREG_CONN_STATUS         0x0001

# define UHCI_FRAME_LIST_ENTRY_TYPE_TD 0
# define UHCI_FRAME_LIST_ENTRY_TYPE_QH 1

struct uhci_frame_list_entry {
    unsigned enable : 1; /* 0 = valid, 1 = empty */
    unsigned type   : 1; /* 0 = TD, 1 = QH */
    int reserved    : 2;
    uint32_t ptr    : 28;
} __attribute__((packed));

struct uhci_qh {
    volatile struct uhci_frame_list_entry horiz_link_ptr;
    volatile struct uhci_frame_list_entry element_link_ptr;
    uint64_t padding;

    /* Not accessible by the controller; for kernel use only */
    uint64_t data_prev;
    uint64_t data_first_td;
} __attribute__((aligned(16))) __attribute__((packed));

# define UHCI_TD_BITSTUFF_ERR 1 << 17
# define UHCI_TD_CRCTO_ERR    1 << 18
# define UHCI_TD_NAK_ERR      1 << 19
# define UHCI_TD_BABBLE_ERR   1 << 20
# define UHCI_TD_BUF_ERR      1 << 21
# define UHCI_TD_STALLED      1 << 22
# define UHCI_TD_ACTIVE       1 << 23
# define UHCI_TD_IOC          1 << 24
# define UHCI_TD_SPD          1 << 29

# define UHCI_TD_ERR_MASK \
     (UHCI_TD_BITSTUFF_ERR | UHCI_TD_CRCTO_ERR | UHCI_TD_NAK_ERR | UHCI_TD_BABBLE_ERR | UHCI_TD_BUF_ERR | UHCI_TD_STALLED)

enum uhci_td_pid { UHCI_TD_PID_OUT = 0xE1, UHCI_TD_PID_IN = 0x69, UHCI_TD_PID_SETUP = 0x2D };

struct uhci_td_packet_header {
    enum uhci_td_pid packet_type : 8; /* See UHCI_TD_PID_* */
    uint8_t device_address       : 7; /* USB device address (0-127) */
    uint8_t endpoint_number      : 4; /* Endpoint number (0-15) */
    uint8_t data_toggle          : 1; /* Data toggle (0 or 1) */
    uint16_t reserved            : 1;
    uint16_t max_length          : 11; /* Maximum length of data to transfer (in bytes) */
} __attribute__((packed));

struct uhci_td_status {
    unsigned actual_length         : 11; /* Actual length of data transferred (in bytes) */
    int reserved2                  : 6;
    unsigned bitstuff_error        : 1;
    unsigned crc_timeout_error     : 1;
    unsigned non_ack_received      : 1;
    unsigned babble_detected       : 1;
    unsigned data_buffer_error     : 1;
    unsigned stalled               : 1;
    unsigned active                : 1; /* This packet waits to be transferred by UHCI controller */
    unsigned interrupt_on_complete : 1;
    unsigned isochronous           : 1;
    unsigned low_speed             : 1; /* 1 = low speed device */
    unsigned error_count           : 2; /* Number of errors (max 3) */
    unsigned short_packet_detect   : 1; /* 1 = if SPD, continue execution from horizontal QH pointer */
    int reserved1                  : 2;
} __attribute__((packed));

struct uhci_td {
    volatile struct uhci_td_next {
        unsigned terminate   : 1; /* 0 = valid, 1 = doesn't point to anything */
        unsigned type        : 1; /* 0 = transfer descriptor, 1 = queue head */
        unsigned depth_first : 1; /* Controller will continue execution to next TD pointed by this TD */
        unsigned reserved    : 1;
        uint32_t ptr         : 28;
    } __attribute__((packed)) next_td;

    uint32_t status;

    uint32_t packet_header;

    uint32_t buffer_pointer; /* Physical address of data buffer */

    /* Not accessible by the controller; for kernel use only */
    uint64_t data_next;
    uint64_t padding;
} __attribute__((packed));

struct uhci_controller {
    pci_device_t *pci_dev;
    uint16_t io_base; /* I/O base address from PCI BAR */
    physptr_t frame_list_phys;
    struct uhci_frame_list_entry *frame_list_base;

    struct usb_bus bus;

    uint8_t dev_addresses[127]; /* address = index + 1; 1 = used */
    mutex dev_addresses_lock;

    mutex qh_lock;

    /* These must be 16-byte aligned */
    struct uhci_qh qh_1ms __attribute__((aligned(16)));
    struct uhci_qh qh_2ms __attribute__((aligned(16)));
    struct uhci_qh qh_4ms __attribute__((aligned(16)));
    struct uhci_qh qh_8ms __attribute__((aligned(16)));
    struct uhci_qh qh_16ms __attribute__((aligned(16)));
    struct uhci_qh qh_32ms __attribute__((aligned(16)));
} __attribute__((aligned(16)));

typedef struct uhci_controller uhci_controller_t;

typedef uhci_controller_t uhci_host_driver_data_t;

int uhci_init(uhci_controller_t *controller, pci_device_t *pci_dev);

int uhci_insert_qh(uhci_controller_t *controller, struct uhci_qh *qh);

void *uhci_alloc_td();
void *uhci_alloc_qh();

void uhci_free_td(void *td);
void uhci_free_qh(void *qh);

#endif // __KERNEL_SUPPORT_DEV_USB_HOST_UHCI