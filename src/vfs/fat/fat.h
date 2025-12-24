#pragma once

#include "devices/storage/drive.h"

#include <stddef.h>
#include <stdint.h>

typedef enum fat_type {
    FAT_TYPE_ERROR = -1,
    FAT_TYPE_12,
    FAT_TYPE_16,
    FAT_TYPE_32,
} fat_type_t;

struct fat_boot_sector_generic {
    uint8_t jump_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed));

// Shared between FAT12, FAT16, and FAT32
struct fat_boot_sector {
    struct fat_boot_sector_generic generic;

    // Extended boot record; bytes 36-511. This differs between FAT12/16 and FAT32
    uint8_t extended_boot_record[476];
} __attribute__((packed));

fat_type_t fat_detect_type(DRIVE_t *drive, uint64_t partition_start_lba, struct fat_boot_sector *bs);