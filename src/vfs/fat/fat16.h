#pragma once

#include "devices/storage/drive.h"
#include "vfs/fat/fat.h"

#include <stddef.h>
#include <stdint.h>

struct fat16_boot_sector {
    struct fat_boot_sector_generic generic;

    // FAT16 specific
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature; // Must be 0x28 or 0x29
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];

    // Bytes 62-509
    char reserved2[448];

    // Signature
    uint16_t boot_sector_signature; // 0xAA55
} __attribute__((packed));

struct fat16_directory_entry {
    char name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed));

struct fat16_long_directory_entry {
    uint8_t order;
    uint16_t name1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t first_cluster_low;
    uint16_t name3[2];
} __attribute__((packed));

struct filesystem_fat16 {
    DRIVE_t *drive;
    uint32_t partition_start_lba; // Relative to drive sector size, NOT FAT sector size

    uint8_t read_status; // Bitfield: 0x01 = boot sector read
    union {
        struct fat16_boot_sector boot_sector;
        struct fat_boot_sector boot_sector_raw;
    };
};

int fat16_setup(struct filesystem_fat16 *fs, DRIVE_t *drive, uint32_t partition_start_lba);