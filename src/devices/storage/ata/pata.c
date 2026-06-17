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
    size_t word_count = bytes / 2;
    
    for (size_t i = 0; i < word_count; i++) {
        uint16_t word = inw(dev->channel->cmd_base + ATA_REG_DATA);
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

    if (!dev) {
        kout(KERNEL_SEVERE_FAULT, "Error: DRIVE internal_data is NULL in pata_read_sectors_func\n");
        return -1;
    }

    if (sectors == 0) {
        return 0;
    }

    // Lock bus
    int res = ide_select_bus(dev);
    if (res != 0) {
        kout(KERNEL_EXTERNAL_FAULT, "Error: could not select bus for PATA device\n");
        return -1;
    }

    outb(dev->channel->cmd_base + ATA_REG_HDDEVSEL, 0xA0 | 0x40 | (dev->drive_no << 4));
    inb(dev->channel->ctrl_base); // 400ns delay
    inb(dev->channel->ctrl_base);
    inb(dev->channel->ctrl_base);
    inb(dev->channel->ctrl_base);

    uint32_t bytes_per_sector = drive->logical_sector_size;
    size_t total_bytes = sectors * bytes_per_sector;
    uint32_t sector_count = (uint32_t)sectors;

    // Construct READ(16) command
    uint8_t read_cmd[16] = { ATA_CMD_IDENTIFY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
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

    outb(dev->channel->cmd_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    // Wait for completion
    while ((inb(dev->channel->cmd_base + ATA_REG_STATUS) & ATA_SR_BSY)) {
        pit_sleep(1);
    }

    // Read data
    pata_read_data(dev, buffer, total_bytes);

    // Unlock bus
    ide_unlock_bus(dev);

    return (ssize_t)total_bytes;
}
