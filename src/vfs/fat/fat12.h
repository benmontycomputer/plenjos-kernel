#pragma once

#include "devices/storage/drive.h"
#include "vfs/fat/fat.h"
#include "vfs/fat/fat16.h"
#include "vfs/vfs.h"

#include <stddef.h>
#include <stdint.h>

#define FAT12_CLUSTER_FREE 0x000
#define FAT12_CLUSTER_EOC  0xFF8
#define FAT12_CLUSTER_BAD  0xFF7

#define FAT_ATTR_LONG_NAME 0x0F
#define FAT_ATTR_DIRECTORY 0x10

// Uses same boot sector structure as FAT16
// All fields are in FAT sectors; translation is handled by fat12_drive_read()
struct filesystem_fat12 {
    DRIVE_t *drive;
    uint32_t partition_start_lba;

    uint8_t read_status; // Bitfield: 0x01 = boot sector read
    union {
        struct fat16_boot_sector boot_sector;
        struct fat_boot_sector boot_sector_raw;
    };

    uint32_t factor; // (size of drive sector) / (size of FAT sector)

    uint32_t fat_start_lba;          // Relative to partition start; uses FAT sector size
    uint32_t cluster_heap_start_lba; // Relative to partition start; uses FAT sector size
    uint32_t root_dir_start_lba;     // Relative to partition start; uses FAT sector size

    uint32_t fat_sectors;      // Uses FAT sector size
    uint32_t root_dir_sectors; // Uses FAT sector size

    uint32_t total_sectors;  // Total sectors in the FAT12 filesystem
    uint32_t total_clusters; // Total clusters in the FAT12 filesystem
};

typedef struct vfs_fat12_cache_node_data {
    struct filesystem_fat12 *fs;
    uint64_t start_cluster;
    uint64_t unused[2];
} __attribute__((packed)) vfs_fat12_cache_node_data_t;

typedef struct vfs_fat12_handle_instance_data {
    // struct filesystem_fat12 *fs; // This can be found from the cache node data
    // uint64_t start_cluster; // This can be found from the cache node data
    uint64_t current_cluster; // 16 bits; Current cluster being read
    uint64_t cluster_pos;     // 64 (32?) bits; Offset within current cluster
    uint64_t cluster_index;   // 32 bits; Index of current_cluster within the file
    uint64_t seek_pos;
} __attribute__((packed)) vfs_fat12_handle_instance_data_t;

typedef struct vfs_filesystem_fat12_instance_data {
    struct filesystem_fat12 *fs;
    uint64_t unused[3];
} __attribute__((packed)) vfs_filesystem_fat12_t_instance_data;

int fat12_get_vfs_filesystem(struct filesystem_fat12 *fs, vfs_filesystem_t *out_fs);
int fat12_setup(struct filesystem_fat12 *fs, DRIVE_t *drive, uint32_t partition_start_lba);