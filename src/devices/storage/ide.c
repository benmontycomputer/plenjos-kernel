#include "devices/storage/ide.h"

#include "arch/x86_64/irq.h"
#include "devices/io/ports.h"
#include "devices/pci/pci.h"
#include "devices/storage/ata/ata.h"
#include "kernel.h"
#include "lib/lock.h"
#include "lib/stdio.h"
#include "timer/pit.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// https://wiki.osdev.org/ATA_PIO_Mode

// TODO: some of this might not work on real hardware, as QEMU is much more lenient on delays/timings between commands.
// However, I also don't own any IDE hardware to test on.

struct ide_device legacy_ide_devices[4]   = { 0 }; // Primary master, primary slave, secondary master, secondary slave
struct ide_channel legacy_ide_channels[2] = {
    { .cmd_base    = IDE_PRIMARY_CMD_BASE,
     .ctrl_base   = IDE_PRIMARY_CTRL_BASE,
     .irq_no      = 14,
     .irq_cnt     = ATOMIC_VAR_INIT(0),
     .last_status = ATOMIC_VAR_INIT(0),
     .lock        = MUTEX_INIT,
     .selected    = -1 },
    { .cmd_base    = IDE_SECONDARY_CMD_BASE,
     .ctrl_base   = IDE_SECONDARY_CTRL_BASE,
     .irq_no      = 15,
     .irq_cnt     = ATOMIC_VAR_INIT(0),
     .last_status = ATOMIC_VAR_INIT(0),
     .lock        = MUTEX_INIT,
     .selected    = -1 },
};

static void ide_lock_channel(struct ide_channel *channel) {
    mutex_lock(&channel->lock);
}

static void ide_unlock_channel(struct ide_channel *channel) {
    mutex_unlock(&channel->lock);
}

int ide_lock_bus(struct ide_device *dev) {
    if (!dev || !dev->channel) {
        printf("Kernel Programming Error: ide_lock_bus called with NULL device or channel\n");
        return -1; // Invalid device
    }

    ide_lock_channel(dev->channel);

    return 0;
}

int ide_unlock_bus(struct ide_device *dev) {
    if (!dev || !dev->channel) {
        printf("Kernel Programming Error: ide_unlock_bus called with NULL device or channel\n");
        return -1; // Invalid device
    }

    ide_unlock_channel(dev->channel);

    return 0;
}

enum ata_device_type ide_probe_device(struct ide_channel *channel, int drive, struct ide_device *dev) {
    dev->flags = drive ? IDE_DEVICE_FLAG_SLAVE : 0;

    enum ata_device_type type;
    uint8_t status
        = inb(channel->cmd_base + ATA_REG_STATUS); // Check for floating bus; must be done at very beginning because
                                                   // writes can cause a floating bus to be at an arbitrary value

    if (status == 0xFF) {
        printf("WARN: Floating bus detected on IDE channel (io=%x, ctrl=%x, drive=%d)\n", channel->cmd_base,
               channel->ctrl_base, drive);
        return ATADEV_NONE; // No device present (floating bus)
    }

    /* Select drive */
    outb(channel->cmd_base + ATA_REG_HDDEVSEL, 0xA0 | (drive << 4));
    inb(channel->ctrl_base); // 400ns delay
    inb(channel->ctrl_base);
    inb(channel->ctrl_base);
    inb(channel->ctrl_base);

    /* Clear registers */
    outb(channel->cmd_base + ATA_REG_SECCOUNT, 0);
    outb(channel->cmd_base + ATA_REG_LBA0, 0);
    outb(channel->cmd_base + ATA_REG_LBA1, 0);
    outb(channel->cmd_base + ATA_REG_LBA2, 0);

    /* Send IDENTIFY command */
    outb(channel->cmd_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    pit_sleep(1); // Wait 1ms for device to respond

    /* Read status */
    status = inb(channel->cmd_base + ATA_REG_STATUS);
    if (status == 0) {
        return ATADEV_NONE; // No device
    }

    int st = ata_wait_bsy_read_status(channel->cmd_base);

    if (st < 0) {
        return ATADEV_NONE; // Timeout waiting for BSY to clear
    } else {
        status = (uint8_t)st;
    }

    /* Check LBA mid/high */
    uint8_t lba1 = inb(channel->cmd_base + ATA_REG_LBA1);
    uint8_t lba2 = inb(channel->cmd_base + ATA_REG_LBA2);

    if (status & ATA_SR_ERR) {
        // Check for ATAPI device

        if (lba1 == 0x14 && lba2 == 0xEB) {
            type        = ATADEV_PATAPI; // ATAPI device
            dev->flags |= IDE_DEVICE_FLAG_ATAPI;
        } else if (lba1 == 0x69 && lba2 == 0x96) {
            type        = ATADEV_SATAPI; // SATA ATAPI device
            dev->flags |= IDE_DEVICE_FLAG_ATAPI;
        } else {
            return ATADEV_NONE; // Unknown device
        }

        // Now, send the IDENTIFY PACKET command (for packet devices)
        outb(channel->cmd_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
        pit_sleep(1); // Wait 1ms for device to respond
    } else {
        if (lba1 == 0 && lba2 == 0) {
            type = ATADEV_PATA; // PATA device
        } else if (lba1 == 0x3C && lba2 == 0xC3) {
            type = ATADEV_SATA; // SATA device
        } else {
            return ATADEV_NONE; // Unknown device
        }
    }

    if (ata_wait_drq(channel->cmd_base) != 0) {
        printf("Error: PATAPI device on IDE channel (io=%x, ctrl=%x, drive=%d) did not set DRQ after IDENTIFY\n",
               channel->cmd_base, channel->ctrl_base, drive);
        return ATADEV_NONE;
    } else {
        dev->channel = channel;
        dev->drive   = drive;
        dev->type    = type;

        // Read IDENTIFY data
        for (int i = 0; i < 256; i++) {
            dev->identify_raw[i] = inw(channel->cmd_base + ATA_REG_DATA);
        }

        if (dev->flags & IDE_DEVICE_FLAG_ATAPI) {
            // ATAPI device
            atapi_parse_identify(dev);
        } else {
            // ATA device
            ata_parse_identify(dev);
        }
    }

    uint64_t size_out      = dev->numsectors * dev->logical_sector_size;
    const char *size_label = "B";
    if (size_out >= 8192) {
        size_out   /= 1024;
        size_label  = "KiB";
    }
    if (size_out >= 8192) {
        size_out   /= 1024;
        size_label  = "MiB";
    }
    if (size_out >= 8192) {
        size_out   /= 1024;
        size_label  = "GiB";
    }

    printf(
        "Found %s IDE device, size %d%s on IDE channel (io=%x, ctrl=%x, drive=%d). Sector size: %d bytes; %d logical "
        "sectors per physical sector; %p sectors.\n",
        dev->type == ATADEV_PATA     ? "PATA"
        : dev->type == ATADEV_SATA   ? "SATA"
        : dev->type == ATADEV_PATAPI ? "PATAPI"
        : dev->type == ATADEV_SATAPI ? "SATAPI"
                                     : "Unknown",
        (int)size_out, size_label, channel->cmd_base, channel->ctrl_base, drive, dev->logical_sector_size,
        dev->identify.sector_size_info & 0x0F ? (1 << (dev->identify.sector_size_info & 0x0F)) : 1, dev->numsectors);

    switch (type) {
    case ATADEV_PATAPI: {

        break;
    }
    default: {
        printf("Found unsupported ATA device type %d on IDE channel (io=%x, ctrl=%x, drive=%d)\n", type,
               channel->cmd_base, channel->ctrl_base, drive);
        break;
    }
    }

    dev->flags |= IDE_DEVICE_FLAG_PRESENT;

    return type;
}

void ide_irq_routine(registers_t *regs, struct ide_channel *channel) {
    if (!mutex_is_locked(&channel->lock)) {
        // Spurious IRQ?
        printf("KERNEL ERROR/WARN: IDE bus w/ IRQ %d isn't locked, but an IRQ was fired. Spurious IRQ or programming "
               "error?\n",
               (int)channel->irq_no);
        return;
    }

    // Acknowledge IRQ by reading status register
    atomic_store(&channel->last_status, inb(channel->cmd_base + ATA_REG_STATUS));

    // Increment IRQ processing count
    if (atomic_fetch_add_explicit(&channel->irq_cnt, 1, memory_order_acquire) > 0) {
        printf("KERNEL ERROR/WARN: IDE bus w/ IRQ %d has multiple (%d) IRQs being processed simultaneously. IRQs may be "
               "lost. Are we moving too slowly?\n",
               (int)channel->irq_no, (int)atomic_load_explicit(&channel->irq_cnt, memory_order_acquire));
    } else {
        // Successfully acknowledged IRQ
        printf("IDE IRQ %d acknowledged, IRQs being processed: %d\n", (int)channel->irq_no,
               (int)atomic_load_explicit(&channel->irq_cnt, memory_order_acquire));
    }
}

void ide_primary_irq_routine(registers_t *regs) {
    ide_irq_routine(regs, &legacy_ide_channels[0]);
}

void ide_secondary_irq_routine(registers_t *regs) {
    ide_irq_routine(regs, &legacy_ide_channels[1]);
}

void ide_init() {
    // Scanning PCI bus for IDE controllers
    for (uint32_t i = 0; i < pci_mass_storage_controller_count; i++) {
        pci_device_t dev = pci_mass_storage_controllers[i];

        if (dev.subclass_code == PCI_SUBCLASS_IDE) {
            // Initialize IDE controller
            printf("Found IDE Controller: %s %s (Bus %d, Device %d, Function %d)\n", get_vendor_pretty(dev.vendor_id),
                   /* dev.device_name */ "{unknown}", dev.bus, dev.device, dev.function);

            printf("  Mode: %s\n", (dev.prog_if & PCI_PROGIF_IDE_MASTER_DMA) ? "Master/DMA" : "No DMA");

            // If PCI native is supported, these BARs represent the following:
            printf("  BAR0: %x\n", dev.bar0); // Primary channel command block
            printf("  BAR1: %x\n", dev.bar1); // Primary channel control block
            printf("  BAR2: %x\n", dev.bar2); // Secondary channel command block
            printf("  BAR3: %x\n", dev.bar3); // Secondary channel control block
            printf("  BAR4: %x\n", dev.bar4); // Bus master IDE registers
            printf("  BAR5: %x\n", dev.bar5);
            printf("  Primary channel status: %s mode, %s\n",
                   (dev.prog_if & PCI_PROGIF_IDE_C0_PCI_NATIVE_MODE) ? "PCI Native" : "Compatibility",
                   (dev.prog_if & PCI_PROGIF_IDE_C0_SWITCHABLE) ? "Switchable" : "Not Switchable");
            printf("  Secondary channel status: %s mode, %s\n",
                   (dev.prog_if & PCI_PROGIF_IDE_C1_PCI_NATIVE_MODE) ? "PCI Native" : "Compatibility",
                   (dev.prog_if & PCI_PROGIF_IDE_C1_SWITCHABLE) ? "Switchable" : "Not Switchable");
        }

        /* printf("enum: %d\n", ide_probe_device(IDE_PRIMARY_CMD_BASE, IDE_PRIMARY_CTRL_BASE, 0));
        printf("enum: %d\n", ide_probe_device(IDE_PRIMARY_CMD_BASE, IDE_PRIMARY_CTRL_BASE, 1));
        printf("enum: %d\n", ide_probe_device(IDE_SECONDARY_CMD_BASE, IDE_SECONDARY_CTRL_BASE, 0));
        printf("enum: %d\n", ide_probe_device(IDE_SECONDARY_CMD_BASE, IDE_SECONDARY_CTRL_BASE, 1)); */
    }

    irq_register_routine(IDE_PRIMARY_IRQ, ide_primary_irq_routine);
    irq_register_routine(IDE_SECONDARY_IRQ, ide_secondary_irq_routine);

    if (!(ide_probe_device(&legacy_ide_channels[0], 0, &legacy_ide_devices[0])
        || ide_probe_device(&legacy_ide_channels[0], 1, &legacy_ide_devices[1]))) {
        // Device(s) found on primary channel
        printf("Unregistering primary IDE IRQ handler\n");
        irq_unregister_routine(IDE_PRIMARY_IRQ);
    }
    if (ide_probe_device(&legacy_ide_channels[1], 0, &legacy_ide_devices[2])
        || ide_probe_device(&legacy_ide_channels[1], 1, &legacy_ide_devices[3])) {
        // Device(s) found on secondary channel
        printf("Unregistering secondary IDE IRQ handler\n");
        irq_unregister_routine(IDE_SECONDARY_IRQ);
    }
}

// This also serves to lock the bus (as long as 0 is returned)
int ide_select_bus(struct ide_device *dev) {
    if (!dev || !dev->channel) {
        printf("Kernel Programming Error: ide_select_bus called with NULL device or channel\n");
        return -1; // Invalid device
    }

    // Valid legacy IDE channel
    ide_lock_bus(dev);

    if (dev->channel->selected != dev->drive) {
        outb(dev->channel->cmd_base + ATA_REG_HDDEVSEL, 0xA0 | (dev->drive << 4));
        inb(dev->channel->ctrl_base); // 400ns delay
        inb(dev->channel->ctrl_base);
        inb(dev->channel->ctrl_base);
        inb(dev->channel->ctrl_base);

        dev->channel->selected = dev->drive;
    }

    return 0;
}

int ide_wait_for_irq(struct ide_device *dev) {
    while (atomic_load_explicit(&dev->channel->irq_cnt, memory_order_acquire) == 0) {
        // Wait for IRQ to be processed
        // TODO: this might need to be something else if the driver runs on a different core from the IRQ handler
        asm volatile("hlt");
    }
    return 0;
}