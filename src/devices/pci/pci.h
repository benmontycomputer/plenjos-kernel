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