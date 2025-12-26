#include "fat16.h"

#include "lib/stdio.h"

int fat16_setup(struct filesystem_fat16 *fs, DRIVE_t *drive, uint32_t partition_start_lba) {
    if (!fs || !drive) {
        return -1;
    }

    fs->drive = drive;

    if (!(fs->read_status & 0x01)) {
        printf("FAT16: boot sector must be read before setup\n");
        return -1;
        /* // Read boot sector and detect type
        fat_type_t res = fat_detect_type(drive, partition_start_lba, &fs->boot_sector_raw);
        if (res != FAT_TYPE_16) {
            printf("Error: not a FAT16 filesystem (type: %s)\n", res == FAT_TYPE_32   ? "FAT32"
                                                                : res == FAT_TYPE_12 ? "FAT12"
                                                                                    : "UNKNOWN");
            return -1;
        }

        fs->read_status |= 0x01; // Mark boot sector as read */
    }

    if (fs->boot_sector.generic.fat_size_16 == 0) {
        // Not a FAT16 filesystem
        printf("Error: Not a FAT16 filesystem (fat_size_16 is 0)\n");
        return -1;
    }

    if (fs->boot_sector.boot_sector_signature != 0xAA55) {
        // Invalid boot sector signature
        printf("Error: Invalid FAT16 boot sector signature: %.4x\n", fs->boot_sector.boot_sector_signature);
        return -1;
    }

    if (fs->boot_sector.generic.bytes_per_sector != drive->logical_sector_size) {
        printf("Warning: FAT16 bytes per sector (%u) does not match drive logical sector size (%u)\n",
               fs->boot_sector.generic.bytes_per_sector, drive->logical_sector_size);
    }

    fs->partition_start_lba = partition_start_lba;

    printf("FAT16 filesystem setup successful. Size: %u bytes\n", fs->boot_sector.generic.total_sectors_16 * fs->boot_sector.generic.bytes_per_sector);

    return 0;
}