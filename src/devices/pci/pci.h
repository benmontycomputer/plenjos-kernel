/**
 * @file pci.h
 * @brief PCI Bus Support Header
 *
 * Header file for PCI bus support. This does not require any other device drivers.
 */

#include "devices/manager.h"

#pragma once

#ifdef __KERNEL_SUPPORT_DEV_PCI

# include "plenjos/dev/pci.h"

# include <stdint.h>

# define PCI_CONFIG_ADDRESS 0xCF8
# define PCI_CONFIG_DATA    0xCFC

# define PCI_MAX_DEVICES 256

# define PCI_REG_CMD           0x04
# define PCI_REG_INTERRUPT_PIN 0x3C
# define PCI_REG_INTERRUPT_NO  0x3D

typedef enum {
    PCI_CMD_IO_SPACE                 = 1 << 0,
    PCI_CMD_MEMORY_SPACE             = 1 << 1,
    PCI_CMD_BUS_MASTER               = 1 << 2,
    PCI_CMD_SPECIAL_CYCLES           = 1 << 3,
    PCI_CMD_MEM_WRITE_INV            = 1 << 4,
    PCI_CMD_VGA_PALETTE_SNOOP        = 1 << 5,
    PCI_CMD_PARITY_ERROR_RESPONSE    = 1 << 6,
    PCI_CMD_WAIT_CYCLE_CONTROL       = 1 << 7,
    PCI_CMD_SERR_ENABLE              = 1 << 8,
    PCI_CMD_FAST_BACK_TO_BACK_ENABLE = 1 << 9,
    PCI_CMD_INTERRUPT_DISABLE        = 1 << 10
} pci_command_bits_t;

/* PCI_CONFIG_ADDRESS register layout:
---------------------------------------------------------------------
|   31   | 30 - 24  | 23 - 16 | 15 - 11  | 10 - 8  | 7 - 2  | 1 - 0 |
---------------------------------------------------------------------
|  Enable | Reserved | Bus #   | Device # | Func # | Offset |  00   |
---------------------------------------------------------------------
*/

// https://wiki.osdev.org/PCI#The_PCI_Bus
typedef enum {
    PCI_VENDOR_INTEL    = 0x8086,
    PCI_VENDOR_REALTEK  = 0x10EC,
    PCI_VENDOR_BROADCOM = 0x14E4,
    PCI_VENDOR_AMD      = 0x1022,
    PCI_VENDOR_NVIDIA   = 0x10DE,
    PCI_VENDOR_VIA      = 0x1106,
    PCI_VENDOR_ATT      = 0x1259,
    PCI_VENDOR_QUALCOMM = 0x17CB,
    PCI_VENDOR_AQUANTIA = 0x1D6A,
    PCI_VENDOR_MELLANOX = 0x15B3,
    PCI_VENDOR_VMWARE   = 0x15AD,
    PCI_VENDOR_VIRTIO   = 0x1AF4,
    PCI_VENDOR_QEMU     = 0x1234
} pci_vendor_t;

// Top level pci class codes
typedef enum {
    PCI_CLASS_UNCLASSIFIED        = 0x00,
    PCI_CLASS_MASS_STORAGE        = 0x01,
    PCI_CLASS_NETWORK             = 0x02,
    PCI_CLASS_DISPLAY             = 0x03,
    PCI_CLASS_MULTIMEDIA          = 0x04,
    PCI_CLASS_MEMORY              = 0x05,
    PCI_CLASS_BRIDGE              = 0x06,
    PCI_CLASS_SIMPLE_COMM         = 0x07,
    PCI_CLASS_BASE_SYSTEM_PERIPH  = 0x08,
    PCI_CLASS_INPUT_DEVICE        = 0x09,
    PCI_CLASS_DOCKING_STATION     = 0x0A,
    PCI_CLASS_PROCESSOR           = 0x0B,
    PCI_CLASS_SERIAL_BUS          = 0x0C,
    PCI_CLASS_WIRELESS            = 0x0D,
    PCI_CLASS_INTELLIGENT_IO      = 0x0E,
    PCI_CLASS_SATELLITE           = 0x0F,
    PCI_CLASS_ENCRYPTION          = 0x10,
    PCI_CLASS_SIGNAL_PROCESSING   = 0x11,
    PCI_CLASS_PROCESSING_ACCEL    = 0x12,
    PCI_CLASS_NON_ESSENTIAL_INSTR = 0x13,
    PCI_CLASS_CO_PROCESSOR        = 0x40,
    PCI_CLASS_VENDOR_SPECIFIC     = 0xFF
} pci_class_t;

// Example: Mass Storage subclasses (0x01)
typedef enum {
    PCI_SUBCLASS_SCSI       = 0x00,
    PCI_SUBCLASS_IDE        = 0x01,
    PCI_SUBCLASS_FLOPPY     = 0x02,
    PCI_SUBCLASS_IPI        = 0x03,
    PCI_SUBCLASS_RAID       = 0x04,
    PCI_SUBCLASS_ATA        = 0x05,
    PCI_SUBCLASS_SERIAL_ATA = 0x06,
    PCI_SUBCLASS_SAS        = 0x07,
    PCI_SUBCLASS_NVM        = 0x08,
    PCI_SUBCLASS_OTHER      = 0x80
} pci_subclass_mass_storage_t;

# define PCI_PROGIF_IDE_C0_PCI_NATIVE_MODE 1 << 0
# define PCI_PROGIF_IDE_C0_SWITCHABLE      1 << 1
# define PCI_PROGIF_IDE_C1_PCI_NATIVE_MODE 1 << 2
# define PCI_PROGIF_IDE_C1_SWITCHABLE      1 << 3
# define PCI_PROGIF_IDE_MASTER_DMA         1 << 7

// Example: Programming Interface for Serial ATA (class 0x01, subclass 0x06)
typedef enum {
    PCI_PROGIF_SATA_VENDOR_SPECIFIC = 0x00,
    PCI_PROGIF_SATA_AHCI_1_0        = 0x01,
    PCI_PROGIF_SATA_STORAGE_BUS     = 0x02
} pci_progif_sata_t;

// Subclass codes for Class 0x06
typedef enum {
    PCI_SUBCLASS_BRIDGE_HOST           = 0x00,
    PCI_SUBCLASS_BRIDGE_ISA            = 0x01,
    PCI_SUBCLASS_BRIDGE_EISA           = 0x02,
    PCI_SUBCLASS_BRIDGE_MCA            = 0x03,
    PCI_SUBCLASS_BRIDGE_PCI_TO_PCI     = 0x04, // normal PCI-to-PCI
    PCI_SUBCLASS_BRIDGE_PCMCIA         = 0x05,
    PCI_SUBCLASS_BRIDGE_NUBUS          = 0x06,
    PCI_SUBCLASS_BRIDGE_CARDBUS        = 0x07,
    PCI_SUBCLASS_BRIDGE_RACEWAY        = 0x08,
    PCI_SUBCLASS_BRIDGE_PCI_TO_PCI_ALT = 0x09, // semi-transparent
    PCI_SUBCLASS_BRIDGE_INFINIBAND_PCI = 0x0A,
    PCI_SUBCLASS_BRIDGE_OTHER          = 0x80
} pci_subclass_bridge_t;

// Programming-interface values (ProgIF) for certain subclasses
typedef enum {
    // For PCI-to-PCI Bridge (Subclass 0x04)
    PCI_PROGIF_BRIDGE_PCI_TO_PCI_NORMAL_DECODE      = 0x00,
    PCI_PROGIF_BRIDGE_PCI_TO_PCI_SUBTRACTIVE_DECODE = 0x01,

    // For RACEway Bridge (Subclass 0x08)
    PCI_PROGIF_BRIDGE_RACEWAY_TRANSPARENT_MODE = 0x00,
    PCI_PROGIF_BRIDGE_RACEWAY_ENDPOINT_MODE    = 0x01,

    // For PCI-to-PCI Bridge (Subclass 0x09 – semi-transparent)
    PCI_PROGIF_BRIDGE_PCI_TO_PCI_SEMI_PRIMARY   = 0x40, // Primary bus toward host CPU
    PCI_PROGIF_BRIDGE_PCI_TO_PCI_SEMI_SECONDARY = 0x80  // Secondary bus toward host CPU
} pci_progif_bridge_t;

typedef enum { PCI_SUBCLASS_SERIAL_BUS_CONTROLLER_USB = 0x03 } pci_subclass_serial_bus_controller_t;
typedef enum {
    // For USB controllers
    PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_UHCI = 0x00,
    PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_OHCI = 0x10,
    PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_EHCI = 0x20,
    PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_XHCI = 0x30,
    PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_UNSPECIFIED     = 0x80,
    PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_DEVICE          = 0xFE
} pci_progif_serial_bus_controller_t;

extern pci_device_t pci_devices[PCI_MAX_DEVICES];
extern uint32_t pci_device_count;

const char *get_vendor_pretty(pci_vendor_t vendor);
const char *get_class_pretty(pci_class_t class_code);
const char *get_mass_storage_subclass_pretty(pci_subclass_mass_storage_t subclass);
const char *get_bridge_subclass_pretty(pci_subclass_bridge_t subclass);
const char *get_subclass_pretty(pci_device_t *dev);
void pci_print_device_info(pci_device_t *dev);

void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

int pci_set_command_bits(uint8_t bus, uint8_t device, uint8_t function, uint16_t bits);
int pci_clear_command_bits(uint8_t bus, uint8_t device, uint8_t function, uint16_t bits);

void pci_scan();

int pci_scan_process_serial_bus_controller(pci_device_t *dev);

#endif // __KERNEL_SUPPORT_DEV_PCI