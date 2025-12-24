#include "atapi.h"

#include "ata.h"
#include "devices/io/ports.h"
#include "devices/storage/ide.h"
#include "lib/stdio.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int atapi_parse_identify(struct ide_device *dev) {
    struct atapi_identify *atapi_id = &dev->atapi_identify;

    // Additional parsing can be done here as needed

    return atapi_ready_device(dev);
}

// Make sure to select the device and lock the bus first!
int atapi_send_packet_command(struct ide_device *dev, const uint8_t *packet, size_t packet_len, void *buffer,
                              size_t buffer_len, int is_write, int is_dma) {
    // Implementation to send ATAPI PACKET command with the given packet
    if (packet_len % 2) {
        // Packet length must be even
        printf("Kernel Programming Error: ATAPI packet length must be even\n");
        return -1;
    }

    if (atomic_load_explicit(&dev->channel->irq_cnt, memory_order_acquire) != 0) {
        printf("Kernel Programming Error: ATAPI send_packet_command called while IRQ processing is ongoing\n");
        return -1;
    }

    if (buffer && (uint64_t)buffer % 2 != 0) {
        printf("Kernel Programming Error: ATAPI data buffer must be word-aligned\n");
        return -1;
    }

    outb(dev->channel->cmd_base + ATA_REG_FEATURES, is_dma ? 0x01 : 0x00); // Enable DMA if is_dma

    // Sectorcount and LBA low registers are currently unused. Send max_byte_count in LBA mid and high.
    uint16_t max_byte_count = (uint16_t)((buffer_len > 0xFFFF) ? 0xFFFF : buffer_len);
    outb(dev->channel->cmd_base + ATA_REG_LBA1, (uint8_t)(max_byte_count & 0xFF));        // LBA mid
    outb(dev->channel->cmd_base + ATA_REG_LBA2, (uint8_t)((max_byte_count >> 8) & 0xFF)); // LBA high

    outb(dev->channel->cmd_base + ATA_REG_COMMAND, ATA_CMD_PACKET); // PACKET command

    // Wait for an IRQ, or wait for BSY to clear and DRQ to set
    // For simplicity, we'll just poll here (not ideal)

    /* ide_wait_for_irq(dev);
    uint8_t status = atomic_load_explicit(&dev->channel->last_status, memory_order_acquire);
    atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);

    if (status & ATA_SR_DF) {
        // TODO: Handle device fault
        printf("Error: ATAPI device reported fatal device error after PACKET command\n");
        return -1;
    }

    if (status & ATA_SR_ERR) {
        printf("Error: ATAPI device reported error after PACKET command\n");
        return -1;
    } */

    uint8_t status;

    if ((status = ata_wait_bsy_read_status(dev->channel->cmd_base)) < 0) {
        printf("Error: ATAPI device did not clear BSY after PACKET command\n");
        return -1;
    }

    if (status & ATA_SR_DF) {
        // TODO: Handle device fault
        printf("Error: ATAPI device reported fatal device error after PACKET command\n");
        return -1;
    }

    if (status & ATA_SR_ERR) {
        printf("Error: ATAPI device reported error after PACKET command\n");
        return -1;
    }

    if (ata_wait_drq(dev->channel->cmd_base) != 0) {
        printf("Error: ATAPI device errored or did not set DRQ after PACKET command\n");
        return -1;
    }

    for (size_t i = 0; i < packet_len; i += 2) {
        uint16_t word = packet[i];
        if (i + 1 < packet_len) {
            word |= ((uint16_t)packet[i + 1]) << 8;
        }
        outw(dev->channel->cmd_base + ATA_REG_DATA, word);
    }

    while (true) {
        // TODO: timeout?
        ide_wait_for_irq(dev);

        uint8_t status = atomic_load_explicit(&dev->channel->last_status, memory_order_acquire);

        if (status & ATA_SR_ERR) {
            printf("Error: ATAPI device reported error after PACKET command\n");
            atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);
            return -1;
        }

        if (!(status & ATA_SR_BSY) && !(status & ATA_SR_DRQ)) {
            // Transfer complete
            break;
        }

        if (is_dma) {
            // Transfer complete
            break;
        }

        // Read LBA mid/high to get byte count
        uint8_t lba1       = inb(dev->channel->cmd_base + ATA_REG_LBA1);
        uint8_t lba2       = inb(dev->channel->cmd_base + ATA_REG_LBA2);
        uint16_t bytecount = ((uint16_t)lba2 << 8) | (uint16_t)lba1;
        uint16_t wordcount = bytecount / 2; // Round up to words

        if (bytecount % 2) {
            printf("Warning: ATAPI device reported odd byte count (%d); ignoring last byte\n", bytecount);
        }

        if (wordcount > 0) {
            if (is_write) {
                // Write data to device
                for (uint16_t i = 0; i < wordcount; i++) {
                    // Delay this as long as possible to give us the best chance at detecting overlapping IRQs, but it
                    // must ultimately be called before the last word is transfered, because the last word can trigger
                    // another IRQ letting us know that we are done, and not calling this before that would lead to a
                    // false positive.
                    if (i == wordcount - 1) {
                        atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);
                    }

                    uint16_t word = ((uint16_t *)buffer)[i];
                    outw(dev->channel->cmd_base + ATA_REG_DATA, word);
                }
            } else {
                // Read data from device
                for (uint16_t i = 0; i < wordcount; i++) {
                    // Delay this as long as possible to give us the best chance at detecting overlapping IRQs, but it
                    // must ultimately be called before the last word is transfered, because the last word can trigger
                    // another IRQ letting us know that we are done, and not calling this before that would lead to a
                    // false positive.
                    if (i == wordcount - 1) {
                        atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);
                    }

                    uint16_t word           = inw(dev->channel->cmd_base + ATA_REG_DATA);
                    ((uint16_t *)buffer)[i] = word;
                }
            }
        } else {
            atomic_fetch_sub_explicit(&dev->channel->irq_cnt, 1, memory_order_release);
        }
    }

    atomic_store_explicit(&dev->channel->last_status, 0, memory_order_release);
    atomic_store_explicit(&dev->channel->irq_cnt, 0, memory_order_release);
    return 0;
}

// We don't strictly need to lock the bus here; this is called during initialization. However, our select function does
// it anyways.
int atapi_ready_device(struct ide_device *dev) {
    // Implementation to ready the ATAPI device (e.g., check status, wait for readiness)

    int res = ide_select_bus(dev);

    if (res != 0) {
        printf("Error: could not select bus for ATAPI device\n\n");
        goto finally;
    }

    /* 1. IDENTIFY PACKET has already been handled and parsed */

    /* 2. INQUIRY command */
    uint8_t inquiry_packet[12] = { ATAPI_COMMAND_INQUIRY, 0, 0, 0, 0x24, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t inquiry_buffer[36] = { 0 };
    res = atapi_send_packet_command(dev, inquiry_packet, sizeof(inquiry_packet), inquiry_buffer, sizeof(inquiry_buffer),
                                    0, 0);
    if (res != 0) {
        printf("Error: INQUIRY command failed for ATAPI device\n");
        goto finally;
    } else {
        // Parse inquiry_buffer as needed
        printf("ATAPI Device Inquiry Data:\n");
        printf("  Peripheral Device Type: %02x\n", inquiry_buffer[0] & 0x1F);
        printf("  Removable Media: %s\n", (inquiry_buffer[1] & 0x80) ? "Yes" : "No");
        printf("  Vendor ID: %.8s\n", &inquiry_buffer[8]);
        printf("  Product ID: %.16s\n", &inquiry_buffer[16]);
        printf("  Product Revision Level: %.4s\n", &inquiry_buffer[32]);
    }

    uint8_t test_unit_ready_packet[12] = { ATAPI_COMMAND_TEST_UNIT_READY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    res = atapi_send_packet_command(dev, test_unit_ready_packet, sizeof(test_unit_ready_packet), NULL, 0, 0, 0);
    if (res != 0) {
        printf("Error: TEST UNIT READY command failed for ATAPI device\n");

        uint8_t request_sense_packet[12] = { ATAPI_COMMAND_REQUEST_SENSE, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0 };
        uint8_t sense_buffer[18]         = { 0 };
        int res_2 = atapi_send_packet_command(dev, request_sense_packet, sizeof(request_sense_packet), sense_buffer,
                                        sizeof(sense_buffer), 0, 0);
        if (res_2 != 0) {
            res = res_2;
            printf("Error: REQUEST SENSE command failed for ATAPI device\n\n");
            goto finally;
        } else {
            printf("ATAPI Device Sense Data:\n");
            printf("  Sense Key: %02x\n", sense_buffer[2] & 0x0F);
            printf("  Additional Sense Code: %02x\n", sense_buffer[12]);
            printf("  Additional Sense Code Qualifier: %02x\n", sense_buffer[13]);
        }

        goto finally;
    } else {
        printf("ATAPI Device is ready.\n");
    }

    uint8_t read_capacity_packet[12] = { ATAPI_COMMAND_READ_CAPACITY_10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t capacity_buffer[8]       = { 0 };
    res = atapi_send_packet_command(dev, read_capacity_packet, sizeof(read_capacity_packet), capacity_buffer,
                                    sizeof(capacity_buffer), 0, 0);
    if (res != 0) {
        printf("Error: READ CAPACITY command failed for ATAPI device\n\n");
        goto finally;
    } else {
        uint32_t max_lba
            = (capacity_buffer[0] << 24) | (capacity_buffer[1] << 16) | (capacity_buffer[2] << 8) | capacity_buffer[3];
        uint32_t block_size
            = (capacity_buffer[4] << 24) | (capacity_buffer[5] << 16) | (capacity_buffer[6] << 8) | capacity_buffer[7];
        uint64_t disk_size = ((uint64_t)(max_lba + 1)) * ((uint64_t)block_size);
        printf("ATAPI Device Capacity:\n");
        printf("  Max LBA: %u\n", max_lba);
        printf("  Block Size: %u bytes\n", block_size);
        // TODO: re-enable size in GB when SSE is enabled
        // printf("  Total Size: %llu bytes (%.2f GB)\n", disk_size, (double)disk_size / (1024.0 * 1024.0 * 1024.0));
        printf("  Total Size: %p bytes\n", disk_size);

        dev->logical_sector_size  = block_size;
        dev->physical_sector_size = block_size;
        dev->numsectors           = (uint64_t)(max_lba + 1);
    }    

finally:
    int unlock_res = ide_unlock_bus(dev);
    if (unlock_res != 0) {
        printf("Error: could not unlock bus for ATAPI device\n");
        res = unlock_res;
    }

    printf("\n");
    return res;
}

ssize_t atapi_read_sectors_func(struct DRIVE *drive, uint64_t lba, size_t sectors, void *buffer) {
    struct ide_device *dev = (struct ide_device *)drive->internal_data;

    if (!dev) {
        printf("Error: DRIVE internal_data is NULL in atapi_read_sectors_func\n");
        return -1;
    }

    if (sectors == 0) {
        return 0;
    }

    int res = ide_select_bus(dev);
    if (res != 0) {
        printf("Error: could not select bus for ATAPI device\n");
        return -1;
    }

    uint32_t bytes_per_sector = drive->logical_sector_size;
    size_t total_bytes        = sectors * bytes_per_sector;
    uint32_t sector_count     = (uint32_t)sectors;

    // Construct READ(10) packet
    uint8_t read_packet[12] = { ATAPI_COMMAND_READ_10,
                                0,
                                (uint8_t)((lba >> 24) & 0xFF),
                                (uint8_t)((lba >> 16) & 0xFF),
                                (uint8_t)((lba >> 8) & 0xFF),
                                (uint8_t)(lba & 0xFF),
                                0,
                                (uint8_t)((sector_count >> 8) & 0xFF),
                                (uint8_t)(sector_count & 0xFF),
                                0,
                                0,
                                0 };

    // printf("ATAPI READ(10): LBA=%p, Sector Count=%u, Total Bytes=%lu\n", (void *)lba, sector_count, total_bytes);
    res = atapi_send_packet_command(dev, read_packet, sizeof(read_packet), buffer, total_bytes, 0, 0);

    ide_unlock_bus(dev);

    if (res != 0) {
        printf("Error: READ(10) command failed for ATAPI device (reading from lba %p, %lu sectors)\n", (void *)lba, sectors);
        return -1;
    }

    return (ssize_t)total_bytes;
}