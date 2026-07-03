#pragma once

#include "kernel.h"

#include <stddef.h>
#include <stdint.h>

typedef struct DRIVE DRIVE_t;
typedef struct partition partition_t;

typedef enum mbr_partition_type mbr_partition_type_t;

enum mbr_partition_type {
    MBR_PARTITION_TYPE_EMPTY        = 0x00,
    MBR_PARTITION_TYPE_FAT12        = 0x01,
    MBR_PARTITION_TYPE_FAT16_SMALL  = 0x04,
    MBR_PARTITION_TYPE_EXTENDED     = 0x05,
    MBR_PARTITION_TYPE_FAT16_LARGE  = 0x06,
    MBR_PARTITION_TYPE_NTFS         = 0x07, // Also exFAT (and HPFS, QNX, IFS, but we will never see those)
    MBR_PARTITION_TYPE_FAT32        = 0x0B,
    MBR_PARTITION_TYPE_FAT32_LBA    = 0x0C,
    MBR_PARTITION_TYPE_NTFS_LBA     = 0x0E,
    MBR_PARTITION_TYPE_EXTENDED_LBA = 0x0F,
    MBR_PARTITION_TYPE_LINUX_SWAP   = 0x82,
    MBR_PARTITION_TYPE_LINUX_NATIVE = 0x83,
    MBR_PARTITION_TYPE_LINUX_LVM    = 0x8E,
    MBR_PARTITION_TYPE_APPLE_HFS    = 0xAF,
    MBR_PARTITION_TYPE_EFI_SYSTEM   = 0xEF,
    MBR_PARTITION_TYPE_BIOS_BOOT    = 0xEF, // Same as EFI System Partition
    MBR_PARTITION_TYPE_UNKNOWN      = 0xFF,
};

typedef ssize_t (*drive_read_sectors_func_t)(DRIVE_t *drive, uint64_t lba, size_t sectors, void *buffer);
typedef ssize_t (*drive_write_sectors_func_t)(DRIVE_t *drive, uint64_t lba, size_t sectors, const void *buffer);

struct partition {
    DRIVE_t *drive;
    mbr_partition_type_t partition_type : 8;
    uint32_t lba_act;
    partition_t *next;
};

struct DRIVE {
    uint64_t numsectors;
    uint32_t logical_sector_size;
    uint32_t physical_sector_size;

    char model[41];
    char interface[16];

    drive_read_sectors_func_t read_sectors;
    drive_write_sectors_func_t write_sectors;

    // Optional; for convenience
    uint32_t irq;

    endpoint_t *endpoint;
    uint32_t partition_count; // For convenience; DO NOT guarantee that the partitions list extends this far
    partition_t *first_partition;

    void *internal_data;
};

void populate_and_register_drive(DRIVE_t *drive, uint64_t numsectors, uint32_t logical_sector_size,
                                 uint32_t physical_sector_size, char *model, char *interface,
                                 drive_read_sectors_func_t read_sectors, drive_write_sectors_func_t write_sectors,
                                 uint32_t irq, void *internal_data);