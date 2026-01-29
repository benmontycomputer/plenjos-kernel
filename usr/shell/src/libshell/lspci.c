#include "dirent.h"
#include "errno.h"
#include "libshell.h"
#include "plenjos/dev/pci.h"
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

__attribute__((aligned(0x10))) static pci_device_t dev;

static char name_buf[PATH_MAX] = { 0 };

const char *get_vendor_pretty(pci_vendor_t vendor) {
    switch (vendor) {
    case PCI_VENDOR_INTEL:
        return "Intel";
    case PCI_VENDOR_REALTEK:
        return "Realtek";
    case PCI_VENDOR_BROADCOM:
        return "Broadcom";
    case PCI_VENDOR_AMD:
        return "AMD";
    case PCI_VENDOR_NVIDIA:
        return "NVIDIA";
    case PCI_VENDOR_VIA:
        return "VIA";
    case PCI_VENDOR_ATT:
        return "AT&T";
    case PCI_VENDOR_QUALCOMM:
        return "Qualcomm";
    case PCI_VENDOR_AQUANTIA:
        return "Aquantia";
    case PCI_VENDOR_MELLANOX:
        return "Mellanox";
    case PCI_VENDOR_VMWARE:
        return "VMware";
    case PCI_VENDOR_VIRTIO:
        return "Virtio";
    case PCI_VENDOR_QEMU:
        return "QEMU";
    default:
        return "Unknown Vendor";
    }
}

const char *get_class_pretty(pci_class_t class) {
    switch (class) {
    case PCI_CLASS_UNCLASSIFIED:
        return "Unclassified";
    case PCI_CLASS_MASS_STORAGE:
        return "Mass Storage Controller";
    case PCI_CLASS_NETWORK:
        return "Network Controller";
    case PCI_CLASS_DISPLAY:
        return "Display Controller";
    case PCI_CLASS_MULTIMEDIA:
        return "Multimedia Controller";
    case PCI_CLASS_MEMORY:
        return "Memory Controller";
    case PCI_CLASS_BRIDGE:
        return "Bridge Device";
    case PCI_CLASS_SIMPLE_COMM:
        return "Simple Communication Controller";
    case PCI_CLASS_BASE_SYSTEM_PERIPH:
        return "Base System Peripheral";
    case PCI_CLASS_INPUT_DEVICE:
        return "Input Device Controller";
    case PCI_CLASS_DOCKING_STATION:
        return "Docking Station";
    case PCI_CLASS_PROCESSOR:
        return "Processor";
    case PCI_CLASS_SERIAL_BUS:
        return "Serial Bus Controller";
    case PCI_CLASS_WIRELESS:
        return "Wireless Controller";
    case PCI_CLASS_INTELLIGENT_IO:
        return "Intelligent I/O Controller";
    case PCI_CLASS_SATELLITE:
        return "Satellite Communication Controller";
    case PCI_CLASS_ENCRYPTION:
        return "Encryption/Decryption Controller";
    case PCI_CLASS_SIGNAL_PROCESSING:
        return "Signal Processing Controller";
    case PCI_CLASS_PROCESSING_ACCEL:
        return "Processing Accelerator";
    case PCI_CLASS_NON_ESSENTIAL_INSTR:
        return "Non-Essential Instrumentation";
    case PCI_CLASS_CO_PROCESSOR:
        return "Co-Processor";
    case PCI_CLASS_VENDOR_SPECIFIC:
        return "Vendor-Specific Device";
    default:
        return "Unknown Class";
    }
}

const char *get_mass_storage_subclass_pretty(pci_subclass_mass_storage_t subclass) {
    switch (subclass) {
    case PCI_SUBCLASS_SCSI:
        return "SCSI Bus Controller";
    case PCI_SUBCLASS_IDE:
        return "IDE Controller";
    case PCI_SUBCLASS_FLOPPY:
        return "Floppy Disk Controller";
    case PCI_SUBCLASS_IPI:
        return "IPI Bus Controller";
    case PCI_SUBCLASS_RAID:
        return "RAID Controller";
    case PCI_SUBCLASS_ATA:
        return "ATA Controller";
    case PCI_SUBCLASS_SERIAL_ATA:
        return "Serial ATA Controller";
    case PCI_SUBCLASS_SAS:
        return "SAS Controller";
    case PCI_SUBCLASS_NVM:
        return "Non-Volatile Memory Controller";
    case PCI_SUBCLASS_OTHER:
        return "Other Mass Storage Controller";
    default:
        return "Unknown Mass Storage Subclass";
    }
}

const char *get_bridge_subclass_pretty(pci_subclass_bridge_t subclass) {
    switch (subclass) {
    case PCI_SUBCLASS_BRIDGE_HOST:
        return "Host Bridge";
    case PCI_SUBCLASS_BRIDGE_ISA:
        return "ISA Bridge";
    case PCI_SUBCLASS_BRIDGE_EISA:
        return "EISA Bridge";
    case PCI_SUBCLASS_BRIDGE_MCA:
        return "MCA Bridge";
    case PCI_SUBCLASS_BRIDGE_PCI_TO_PCI:
        return "PCI-to-PCI Bridge";
    case PCI_SUBCLASS_BRIDGE_PCMCIA:
        return "PCMCIA Bridge";
    case PCI_SUBCLASS_BRIDGE_NUBUS:
        return "NuBus Bridge";
    case PCI_SUBCLASS_BRIDGE_CARDBUS:
        return "CardBus Bridge";
    case PCI_SUBCLASS_BRIDGE_RACEWAY:
        return "RACEway Bridge";
    case PCI_SUBCLASS_BRIDGE_PCI_TO_PCI_ALT:
        return "Semi-Transparent PCI-to-PCI Bridge";
    case PCI_SUBCLASS_BRIDGE_INFINIBAND_PCI:
        return "InfiniBand to PCI Host Bridge";
    case PCI_SUBCLASS_BRIDGE_OTHER:
        return "Other Bridge Device";
    default:
        return "Unknown Bridge Subclass";
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

void pci_print_device_info(const char *name, pci_device_t *dev) {
    const char *vendor_pretty   = get_vendor_pretty(dev->vendor_id);
    const char *class_pretty    = get_class_pretty(dev->class_code);
    const char *subclass_pretty = get_subclass_pretty(dev);

    printf("%s:\n", name);
    printf(" - Header Type: %.2x\n", dev->header_type);
    printf(" - Bus %.2x, device %.2x, function %.2x\n", dev->bus, dev->device, dev->function);
    printf(" - Vendor ID: %.4x (%s), Device ID: %.4x\n", dev->vendor_id, vendor_pretty, dev->device_id);
    printf(" - Class %.2x (%s), Subclass %.2x (%s), ProgIF %.2x\n", dev->class_code, class_pretty, dev->subclass_code, subclass_pretty, dev->prog_if);
}

int lspci_cmd(int argc, char argv[][CMD_BUFFER_MAX]) {
    DIR *dir = opendir("/dev/pci");
    if (!dir) {
        switch (errno) {
        case ENOENT:
            printf("lspci: directory /dev/pci does not exist.\n");
            break;
        case ENOTDIR:
            printf("lspci: /dev/pci is not a directory.\n");
            break;
        case EACCES:
            printf("lspci: permission denied to open directory /dev/pci.\n");
            break;
        default:
            printf("lspci: failed to open directory /dev/pci (errno %d).\n", errno);
            break;
        }
        return -1;
    }

    strncpy(name_buf, "/dev/pci/", 10);
    name_buf[9] = 0;

    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strlen(entry->d_name) == 0) {
            continue;
        }
        strncpy(name_buf + 9, entry->d_name, PATH_MAX - 9);
        name_buf[PATH_MAX - 1] = 0;

        FILE *file = fopen(name_buf, "r");
        if (!file) {
            switch (errno) {
            case ENOENT:
                printf("lspci: file %s does not exist.\n", name_buf);
                break;
            case EACCES:
                printf("lspci: permission denied to read file %s.\n", name_buf);
                break;
            default:
                printf("lspci: failed to open file %s (errno %d).\n", name_buf, errno);
                break;
            }
            continue;
        }

        if (errno != 0) {
            errno = 0;
        }

        size_t read = fread(&dev, sizeof(dev), 1, file);

        if (read != 1) {
            printf("lspci: warn: short read of file %s (errno %d; expected %d, got %d).\n", name_buf, errno,
                   1, read);
        } else {
            pci_print_device_info(name_buf, &dev);
        }

        if (errno != 0) {
            errno = 0;
        }

        int res = fclose(file);
        if (res) {
            printf("lspci: close of file %s failed (errno %d).\n", name_buf, errno);
            continue;
        }
    }

    switch (errno) {
    case 0:
        break;
    case EBADF:
        printf("lspci: directory stream for /dev/pci is not valid.\n");
        closedir(dir);
        return -1;
    default:
        printf("lspci: error reading directory /dev/pci (errno %d).\n", errno);
        closedir(dir);
        return -1;
    }

    closedir(dir);

    return 0;
}