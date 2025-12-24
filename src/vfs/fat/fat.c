#include "fat.h"

#include "devices/storage/drive.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"

#include <stddef.h>
#include <stdint.h>

// TODO: handle misaligned partition starts relative to FAT sectors?
fat_type_t fat_detect_type(DRIVE_t *drive, uint64_t partition_start_lba, struct fat_boot_sector *bs) {
    if (!drive || !bs) {
        return FAT_TYPE_ERROR;
    }

    uint8_t *buffer = kmalloc_heap(drive->logical_sector_size);
    if (!buffer) {
        printf("OOM Error: could not allocate memory for FAT boot sector read buffer\n");
        return FAT_TYPE_ERROR;
    }

    // Read boot sector
    ssize_t res = drive->read_sectors(drive, partition_start_lba, 1, buffer);
    if (res < 0) {
        printf("Error: Could not read FAT boot sector\n");
        kfree_heap(buffer);
        return FAT_TYPE_ERROR;
    }

    // Dump the buffer contents in hex
    /* for (size_t i = 0; i < 512; i++) {
        if (i % 16 == 0) {
            printf("\n%.4x: ", (unsigned int)i);
        }
        printf("%.2x ", buffer[i]);
    }
    printf("\n"); */

    memcpy(bs, buffer, sizeof(struct fat_boot_sector));
    kfree_heap(buffer);

    if (bs->generic.fat_size_16 != 0) {
        // FAT12 or FAT16
        uint32_t root_dir_sectors
            = ((bs->generic.root_entry_count * 32) + (bs->generic.bytes_per_sector - 1)) / bs->generic.bytes_per_sector;
        uint32_t fat_size = bs->generic.fat_size_16;
        uint32_t total_sectors
            = (bs->generic.total_sectors_16 != 0) ? bs->generic.total_sectors_16 : bs->generic.total_sectors_32;
        uint32_t data_sectors
            = total_sectors
              - (bs->generic.reserved_sector_count + (bs->generic.num_fats * fat_size) + root_dir_sectors);
        uint32_t total_clusters = data_sectors / bs->generic.sectors_per_cluster;
        if (total_clusters < 4085) {
            return FAT_TYPE_12;
        } else {
            return FAT_TYPE_16;
        }
    } else {
        // FAT32
        return FAT_TYPE_32;
    }
}