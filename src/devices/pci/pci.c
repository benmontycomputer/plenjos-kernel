#include "devices/pci/pci.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "vfs/vfs.h"
#include "vfs/kernelfs.h"
#include "memory/kmalloc.h"
#include "devices/io/ports.h"

pci_device_t pci_devices[PCI_MAX_DEVICES];
uint32_t pci_device_count = 0;

pci_device_t pci_mass_storage_controllers[PCI_MAX_MASS_STORAGE_CONTROLLERS];
uint32_t pci_mass_storage_controller_count = 0;

void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint8_t pci_get_header_type(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t header_type_reg = pci_read(bus, device, function, 0x0C);
    return (header_type_reg >> 16) & 0xFF;
}

const char *get_vendor_pretty(pci_vendor_t vendor) {
    switch (vendor) {
        case PCI_VENDOR_INTEL: return "Intel";
        case PCI_VENDOR_REALTEK: return "Realtek";
        case PCI_VENDOR_BROADCOM: return "Broadcom";
        case PCI_VENDOR_AMD: return "AMD";
        case PCI_VENDOR_NVIDIA: return "NVIDIA";
        case PCI_VENDOR_VIA: return "VIA";
        case PCI_VENDOR_ATT: return "AT&T";
        case PCI_VENDOR_QUALCOMM: return "Qualcomm";
        case PCI_VENDOR_AQUANTIA: return "Aquantia";
        case PCI_VENDOR_MELLANOX: return "Mellanox";
        case PCI_VENDOR_VMWARE: return "VMware";
        case PCI_VENDOR_VIRTIO: return "Virtio";
        case PCI_VENDOR_QEMU: return "QEMU";
        default: return "Unknown Vendor";
    }
}

const char *get_class_pretty(pci_class_t class) {
    switch (class) {
        case PCI_CLASS_UNCLASSIFIED: return "Unclassified";
        case PCI_CLASS_MASS_STORAGE: return "Mass Storage Controller";
        case PCI_CLASS_NETWORK: return "Network Controller";
        case PCI_CLASS_DISPLAY: return "Display Controller";
        case PCI_CLASS_MULTIMEDIA: return "Multimedia Controller";
        case PCI_CLASS_MEMORY: return "Memory Controller";
        case PCI_CLASS_BRIDGE: return "Bridge Device";
        case PCI_CLASS_SIMPLE_COMM: return "Simple Communication Controller";
        case PCI_CLASS_BASE_SYSTEM_PERIPH: return "Base System Peripheral";
        case PCI_CLASS_INPUT_DEVICE: return "Input Device Controller";
        case PCI_CLASS_DOCKING_STATION: return "Docking Station";
        case PCI_CLASS_PROCESSOR: return "Processor";
        case PCI_CLASS_SERIAL_BUS: return "Serial Bus Controller";
        case PCI_CLASS_WIRELESS: return "Wireless Controller";
        case PCI_CLASS_INTELLIGENT_IO: return "Intelligent I/O Controller";
        case PCI_CLASS_SATELLITE: return "Satellite Communication Controller";
        case PCI_CLASS_ENCRYPTION: return "Encryption/Decryption Controller";
        case PCI_CLASS_SIGNAL_PROCESSING: return "Signal Processing Controller";
        case PCI_CLASS_PROCESSING_ACCEL: return "Processing Accelerator";
        case PCI_CLASS_NON_ESSENTIAL_INSTR: return "Non-Essential Instrumentation";
        case PCI_CLASS_CO_PROCESSOR: return "Co-Processor";
        case PCI_CLASS_VENDOR_SPECIFIC: return "Vendor-Specific Device";
        default: return "Unknown Class";
    }
}

const char *get_mass_storage_subclass_pretty(pci_subclass_mass_storage_t subclass) {
    switch (subclass) {
        case PCI_SUBCLASS_SCSI: return "SCSI Bus Controller";
        case PCI_SUBCLASS_IDE: return "IDE Controller";
        case PCI_SUBCLASS_FLOPPY: return "Floppy Disk Controller";
        case PCI_SUBCLASS_IPI: return "IPI Bus Controller";
        case PCI_SUBCLASS_RAID: return "RAID Controller";
        case PCI_SUBCLASS_ATA: return "ATA Controller";
        case PCI_SUBCLASS_SERIAL_ATA: return "Serial ATA Controller";
        case PCI_SUBCLASS_SAS: return "SAS Controller";
        case PCI_SUBCLASS_NVM: return "Non-Volatile Memory Controller";
        case PCI_SUBCLASS_OTHER: return "Other Mass Storage Controller";
        default: return "Unknown Mass Storage Subclass";
    }
}

const char *get_bridge_subclass_pretty(pci_subclass_bridge_t subclass) {
    switch (subclass) {
        case PCI_SUBCLASS_BRIDGE_HOST: return "Host Bridge";
        case PCI_SUBCLASS_BRIDGE_ISA: return "ISA Bridge";
        case PCI_SUBCLASS_BRIDGE_EISA: return "EISA Bridge";
        case PCI_SUBCLASS_BRIDGE_MCA: return "MCA Bridge";
        case PCI_SUBCLASS_BRIDGE_PCI_TO_PCI: return "PCI-to-PCI Bridge";
        case PCI_SUBCLASS_BRIDGE_PCMCIA: return "PCMCIA Bridge";
        case PCI_SUBCLASS_BRIDGE_NUBUS: return "NuBus Bridge";
        case PCI_SUBCLASS_BRIDGE_CARDBUS: return "CardBus Bridge";
        case PCI_SUBCLASS_BRIDGE_RACEWAY: return "RACEway Bridge";
        case PCI_SUBCLASS_BRIDGE_PCI_TO_PCI_ALT: return "Semi-Transparent PCI-to-PCI Bridge";
        case PCI_SUBCLASS_BRIDGE_INFINIBAND_PCI: return "InfiniBand to PCI Host Bridge";
        case PCI_SUBCLASS_BRIDGE_OTHER: return "Other Bridge Device";
        default: return "Unknown Bridge Subclass";
    }
}

const char *get_subclass_pretty(pci_device_t *dev) {
    if (dev->class_code == PCI_CLASS_MASS_STORAGE) {
        return get_mass_storage_subclass_pretty((pci_subclass_mass_storage_t)dev->subclass_code);
    } else if (dev->class_code == PCI_CLASS_BRIDGE) {
        return get_bridge_subclass_pretty((pci_subclass_bridge_t)dev->subclass_code);
    } else {
        return "Unknown Subclass";
    }
}

void pci_print_device_info(pci_device_t *dev) {
    const char *vendor_pretty = get_vendor_pretty(dev->vendor_id);
    const char *class_pretty = get_class_pretty(dev->class_code);
    const char *subclass_pretty = get_subclass_pretty(dev);

    printf("PCI Device Info:\n");
    printf("  Header Type: %x\n", dev->header_type);
    printf("  Bus: %x\n", dev->bus);
    printf("  Device: %x\n", dev->device);
    printf("  Function: %x\n", dev->function);
    printf("  Vendor ID: %x (%s)\n", dev->vendor_id, vendor_pretty);
    printf("  Device ID: %x\n", dev->device_id);
    printf("  Class Code: %x (%s)\n", dev->class_code, class_pretty);
    printf("  Subclass Code: %x (%s)\n", dev->subclass_code, subclass_pretty);
    printf("  Programming Interface: %x\n", dev->prog_if);
}

pci_device_t pci_check_device(uint8_t bus, uint8_t device, uint8_t function) {
    pci_device_t dev;
    dev.header_type = 0;
    dev.bus = bus;
    dev.device = device;
    dev.function = function;
    dev.vendor_id = 0xFFFF;
    dev.device_id = 0xFFFF;
    dev.class_code = 0xFF;
    dev.subclass_code = 0xFF;
    dev.prog_if = 0xFF;

    uint32_t vendor_device = pci_read(bus, device, function, 0);

    if (vendor_device == 0xFFFFFFFF) {
        return dev; // No device present
    }

    uint16_t vendor_id = vendor_device & 0xFFFF;
    uint16_t device_id = (vendor_device >> 16) & 0xFFFF;
    uint8_t header_type = pci_get_header_type(bus, device, function);

    dev.header_type = header_type;
    dev.vendor_id = vendor_id;
    dev.device_id = device_id;
    dev.class_code = (pci_read(bus, device, function, 8) >> 24) & 0xFF;
    dev.subclass_code = (pci_read(bus, device, function, 8) >> 16) & 0xFF;
    dev.prog_if = (pci_read(bus, device, function, 8) >> 8) & 0xFF;

    dev.bar0 = pci_read(bus, device, function, 0x10);
    dev.bar1 = pci_read(bus, device, function, 0x14);
    dev.bar2 = pci_read(bus, device, function, 0x18);
    dev.bar3 = pci_read(bus, device, function, 0x1C);
    dev.bar4 = pci_read(bus, device, function, 0x20);
    dev.bar5 = pci_read(bus, device, function, 0x24);

    pci_print_device_info(&dev);

    switch (dev.class_code) {
        case PCI_CLASS_MASS_STORAGE:
            if (pci_mass_storage_controller_count < PCI_MAX_MASS_STORAGE_CONTROLLERS) {
                pci_mass_storage_controllers[pci_mass_storage_controller_count] = dev;
                pci_mass_storage_controller_count++;
            } else {
                printf("Mass storage controller array full, cannot record more controllers.\n");
            }
            break;
        default:
            break;
    }

    return dev;
}

static char hex_to_char(uint8_t val) {
    if (val > 15) return '?';
    if (val < 10) return '0' + val;
    return 'A' + (val - 10);
}

static uint8_t char_to_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0xFF; // Invalid
}

#define PCI_DEV_PREFIX "pci_dev_0x"

ssize_t pci_dev_file_read(kernelfs_node_t *node, void *buf, size_t len) {
    pci_device_t *dev = (pci_device_t *)node->func_args;

    // For simplicity, just copy the pci_device_t structure into the buffer
    size_t copylen = (len > sizeof(pci_device_t)) ? sizeof(pci_device_t) : len;
    memcpy(buf, dev, copylen);

    return copylen;
}

static void pci_add_device_to_array(uint8_t bus, uint8_t device, uint8_t function) {
    pci_devices[pci_device_count] = pci_check_device(bus, device, function);

    // Ex: pci_dev_0000
    char *devaddr = (char *)kmalloc_heap(strlen(PCI_DEV_PREFIX) + 5);

    strncpy(devaddr, PCI_DEV_PREFIX, strlen(PCI_DEV_PREFIX) + 1);
    // TODO: once snprintf is implemented, use that instead
    devaddr[strlen(PCI_DEV_PREFIX) + 0] = hex_to_char((pci_device_count / 16 / 16 / 16) % 16);
    devaddr[strlen(PCI_DEV_PREFIX) + 1] = hex_to_char((pci_device_count / 16 / 16) % 16);
    devaddr[strlen(PCI_DEV_PREFIX) + 2] = hex_to_char((pci_device_count / 16) % 16);
    devaddr[strlen(PCI_DEV_PREFIX) + 3] = hex_to_char((pci_device_count) % 16);
    devaddr[strlen(PCI_DEV_PREFIX) + 4] = 0;

    // kernelfs_create_node("/dev/pci", devaddr, DT_DIR, NULL, pci_dev_file_read, NULL, NULL, &pci_devices[pci_device_count]);

    pci_device_count++;
}

// First, scan for devices using a brute-force approach
void pci_scan() {
    uint16_t bus;
    uint8_t device;

    // kernelfs_create_node("/", "dev", DT_DIR, NULL, NULL, NULL, NULL, NULL);
    // kernelfs_create_node("/dev", "pci", DT_DIR, NULL, NULL, NULL, NULL, NULL);
    // kernelfs_create_node("/dev/pci", "subftest", DT_DIR, NULL, NULL, NULL);

    for (bus = 0; bus < 256; bus++) {
        for (device = 0; device < 32; device++) {
            uint32_t vendor_device = pci_read(bus, device, 0, 0);

            if (vendor_device == 0xFFFFFFFF) {
                continue; // No device present
            }

            uint8_t function = 0;

            if (pci_device_count < PCI_MAX_DEVICES) {
                pci_add_device_to_array(bus, device, function);
            } else {
                printf("PCI device array full, cannot record more devices.\n");
                return;
            }

            if (pci_get_header_type(bus, device, 0) & 0x80) {
                // Multi-function device
                for (function = 1; function < 8; function++) {
                    vendor_device = pci_read(bus, device, function, 0);
                    if (vendor_device != 0xFFFFFFFF) {
                        if (pci_device_count < PCI_MAX_DEVICES) {
                            pci_add_device_to_array(bus, device, function);
                        } else {
                            printf("PCI device array full, cannot record more devices.\n");
                            return;
                        }
                    }
                }
            }
        }
    }
}