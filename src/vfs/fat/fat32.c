#include "fat32.h"

#include "lib/stdio.h"
#include "memory/kmalloc.h"

int fat32_setup(struct filesystem_fat32 *fs, DRIVE_t *drive, uint32_t partition_start_lba) {
    if (!fs || !drive) {
        return -1;
    }

    fs->drive = drive;

    if (!(fs->read_status & 0x01)) {
        printf("FAT32: boot sector must be read before setup\n");
        return -1;
        /* // Read boot sector and detect type
        fat_type_t res = fat_detect_type(drive, partition_start_lba, &fs->boot_sector_raw);
        if (res != FAT_TYPE_32) {
            printf("Error: not a FAT32 filesystem (type: %s)\n", res == FAT_TYPE_16   ? "FAT16"
                                                                 : res == FAT_TYPE_12 ? "FAT12"
                                                                                      : "UNKNOWN");
            return -1;
        }

        fs->read_status |= 0x01; // Mark boot sector as read */
    }

    if (fs->boot_sector.fat_size_32 == 0) {
        // Not a FAT32 filesystem
        printf("Error: Not a FAT32 filesystem (fat_size_32 is 0)\n");
        return -1;
    }

    if (fs->boot_sector.boot_sector_signature != 0xAA55) {
        // Invalid boot sector signature
        printf("Error: Invalid FAT32 boot sector signature: %.4x\n", fs->boot_sector.boot_sector_signature);
        return -1;
    }

    if (fs->boot_sector.generic.bytes_per_sector != drive->logical_sector_size) {
        printf("Warning: FAT32 bytes per sector (%u) does not match drive logical sector size (%u)\n",
               fs->boot_sector.generic.bytes_per_sector, drive->logical_sector_size);
    }

    if (!(fs->read_status & 0x02)) {
        if (fs->boot_sector.fs_info == 0 || fs->boot_sector.fs_info == 0xFFFF) {
            // No valid FSInfo sector
            printf("Warning: No valid FSInfo sector\n");
            memset(&fs->fsinfo, 0, sizeof(struct fat32_fsinfo));
        } else {
            // Read FSInfo sector
            uint8_t *buffer = kmalloc_heap(drive->logical_sector_size);
            if (!buffer) {
                printf("Error: Could not allocate memory for FAT32 FSInfo sector read buffer\n");
                return -1;
            }
            ssize_t res = drive->read_sectors(drive, (uint64_t)partition_start_lba + (uint64_t)fs->boot_sector.fs_info,
                                              1, buffer);
            if (res < 0) {
                printf("Error: Could not read FAT32 FSInfo sector\n");
                kfree_heap(buffer);
                return -1;
            }
            memcpy(&fs->fsinfo, buffer, sizeof(struct fat32_fsinfo));
            kfree_heap(buffer);
        }

        fs->read_status |= 0x02; // Mark FSInfo as read
    }

    if (fs->fsinfo.lead_signature != 0x41615252 || fs->fsinfo.struct_signature != 0x61417272
        || fs->fsinfo.trail_signature != 0xAA550000) {
        printf("Warning: Invalid FAT32 FSInfo signatures\n");
        memset(&fs->fsinfo, 0, sizeof(fs->fsinfo));
    }

    // Calculate total clusters
    uint32_t data_sectors = fs->boot_sector.generic.total_sectors_32
                            - (fs->boot_sector.generic.reserved_sector_count
                               + (fs->boot_sector.generic.num_fats * fs->boot_sector.fat_size_32));
    fs->total_clusters = data_sectors / fs->boot_sector.generic.sectors_per_cluster;

    fs->fat_start_lba          = partition_start_lba + fs->boot_sector.generic.reserved_sector_count;
    fs->cluster_heap_start_lba = fs->fat_start_lba + (fs->boot_sector.generic.num_fats * fs->boot_sector.fat_size_32);
    fs->sectors_per_fat        = fs->boot_sector.fat_size_32;
    fs->total_sectors          = fs->boot_sector.generic.total_sectors_32;
    fs->partition_start_lba    = partition_start_lba;

    printf("FAT32: Total clusters: %u\n", fs->total_clusters);
    printf("FAT32: Free clusters (from FSInfo): %u\n", fs->fsinfo.free_cluster_count);
    printf("FAT32: Next free cluster (from FSInfo): %u\n", fs->fsinfo.next_free_cluster);

    return 0;
}