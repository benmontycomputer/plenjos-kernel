#pragma once

#include "devices/storage/drive.h"
#include "vfs/fat/fat.h"

#include <stddef.h>
#include <stdint.h>

struct fat32_boot_sector {
    struct fat_boot_sector_generic generic;

    // FAT32 specific
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info; // Sector number of FSInfo structure
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature; // Must be 0x28 or 0x29
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];

    // Bytes 90-509
    char reserved2[420];

    // Signature
    uint16_t boot_sector_signature; // 0xAA55
} __attribute__((packed));

struct fat32_fsinfo {
    uint32_t lead_signature; // 0x41615252
    uint8_t reserved1[480];
    uint32_t struct_signature;   // 0x61417272
    uint32_t free_cluster_count; // Number of free clusters; 0xFFFFFFFF if unknown
    uint32_t next_free_cluster;  // Cluster number of the next free cluster; 0xFFFFFFFF if unknown
    uint8_t reserved2[12];
    uint32_t trail_signature; // 0xAA550000
} __attribute__((packed));

struct fat32_directory_entry {
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

struct fat32_long_directory_entry {
    uint8_t order;
    uint16_t name1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t first_cluster_low; // Always 0 for LFN entries
    uint16_t name3[2];
} __attribute__((packed));

struct filesystem_fat32 {
    DRIVE_t *drive;
    uint32_t partition_start_lba;

    uint8_t read_status; // Bit 0 = boot sector read, bit 1 = FSInfo read
    union {
        struct fat32_boot_sector boot_sector;
        struct fat_boot_sector boot_sector_raw;
    };
    struct fat32_fsinfo fsinfo;
    uint32_t fat_start_lba;
    uint32_t cluster_heap_start_lba;
    uint32_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t total_clusters;
};

int fat32_setup(struct filesystem_fat32 *fs, DRIVE_t *drive, uint32_t partition_start_lba);