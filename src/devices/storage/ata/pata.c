#include "pata.h"

#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "ata.h"
#include "devices/io/ports.h"
#include "devices/storage/ide.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "timer/pit.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Read data from PATA device
 */
void pata_read_data(struct ide_device *dev, void *buffer, size_t bytes) {
    // Implementation to read data from PATA device
    if (bytes % 2) {
        kout(KERNEL_PANIC, "pata_read_data(): bytes is odd (should be impossible)!\n");
        return;
    }
    size_t word_count = bytes / 2;

    for (size_t i = 0; i < word_count; i++) {
        uint16_t word           = inw(dev->channel->cmd_base + ATA_REG_DATA);
        ((uint16_t *)buffer)[i] = word;
    }
}

/**
 * Write data to PATA device
 */
void pata_write_data(struct ide_device *dev, const void *buffer, size_t bytes) {
    // Implementation to write data to PATA device
    size_t word_count = bytes / 2;

    for (size_t i = 0; i < word_count; i++) {
        outw(dev->channel->cmd_base + ATA_REG_DATA, ((uint16_t *)buffer)[i]);
    }
}

ssize_t pata_read_sectors_func(DRIVE_t *drive, uint64_t lba, size_t sectors, void *buffer) {
    struct ide_device *dev = (struct ide_device *)drive->internal_data;

    if (lba + sectors > drive->numsectors) {
        kout(KERNEL_SEVERE_EXTERNAL_FAULT,
             "pata_read_sectors_func(): lba + sectors > drive->numsectors! (probably a bug in a filesystem driver)\n");
        return -EIO;
    }

    bool interrupts_were_enabled = are_interrupts_enabled();

    uint32_t *prev_low_dwords = NULL;

    if (!interrupts_were_enabled) {
        if (dev->drive.irq != (uint32_t)-1) {
            prev_low_dwords = kmalloc_heap(sizeof(uint32_t) * 128);
            if (!prev_low_dwords) {
                kout(KERNEL_SEVERE_FAULT, "OOM Error: unable to allocate memory for IOAPIC masks backup\n");
                return -1;
            }
            ioapic_mask_all_except(dev->drive.irq, prev_low_dwords, 128);
        }

        asm volatile("sti");
    }

    int res            = 0;
    size_t total_bytes = 0;

    if (!dev) {
        kout(KERNEL_PANIC, "Error: DRIVE internal_data is NULL in pata_read_sectors_func\n");
        res = -1;
        goto cleanup;
    }

    if (sectors == 0) {
        goto cleanup;
    }

    // Lock bus
    res = ide_select_bus(dev);
    if (res != 0) {
        kout(KERNEL_EXTERNAL_FAULT, "Error: could not select bus for PATA device\n");
        res = -1;
        goto cleanup;
    }

    outb(dev->channel->cmd_base + ATA_REG_HDDEVSEL, 0x40 | (dev->drive_no << 4));
    inb(dev->channel->ctrl_base); // 400ns delay
    inb(dev->channel->ctrl_base);
    inb(dev->channel->ctrl_base);
    inb(dev->channel->ctrl_base);

    uint32_t bytes_per_sector = drive->logical_sector_size;
    if ((sectors * bytes_per_sector) % 2) {
        kout(KERNEL_PANIC, "pata_read_sectors_func(): somehow, total_bytes is odd!\n");
        res = -EIO;
        goto cleanup;
    }
    uint32_t sector_count = (uint32_t)sectors;

    // Send READ(16) command with LBA and sector count
    outb(dev->channel->cmd_base + ATA_REG_SECCOUNT, (uint8_t)((sector_count >> 8) & 0xFF));
    outb(dev->channel->cmd_base + ATA_REG_LBA0, (uint8_t)((lba >> 24) & 0xFF));
    outb(dev->channel->cmd_base + ATA_REG_LBA1, (uint8_t)((lba >> 32) & 0xFF));
    outb(dev->channel->cmd_base + ATA_REG_LBA2, (uint8_t)((lba >> 40) & 0xFF));

    // Send lower half of data
    outb(dev->channel->cmd_base + ATA_REG_SECCOUNT, (uint8_t)(sector_count & 0xFF));
    outb(dev->channel->cmd_base + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(dev->channel->cmd_base + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(dev->channel->cmd_base + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));

    outb(dev->channel->cmd_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS_EXT);
    inb(dev->channel->ctrl_base); // 400ns delay
    inb(dev->channel->ctrl_base);
    inb(dev->channel->ctrl_base);
    inb(dev->channel->ctrl_base);

    size_t sectors_read = 0;

    // Wait for completion
    while (sectors_read < sectors) {
        ide_wait_for_irq(dev);

        uint8_t status = atomic_load_explicit(&dev->channel->last_status, memory_order_acquire);

        if (status & ATA_SR_ERR) {
            atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);
            kout(KERNEL_EXTERNAL_FAULT, "Error: ATAPI device reported error after PACKET command\n");
            res = -EIO;
            goto cleanup;
        }

        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            pata_read_data(dev, buffer, bytes_per_sector);
            sectors_read++;
            total_bytes += bytes_per_sector;
            atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);
        }
    }

cleanup:
    if (interrupts_were_enabled == 0) {
        asm volatile("cli");
    }

    if (prev_low_dwords) {
        ioapic_restore_masks(prev_low_dwords, 128);
        kfree_heap(prev_low_dwords);
    }

    // Unlock bus
    ide_unlock_bus(dev);

    if (res) {
        return (ssize_t)res;
    }
    return (ssize_t)total_bytes;
}
