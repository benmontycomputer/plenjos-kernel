#pragma once

#include <stddef.h>
#include <stdint.h>

#include "drive.h"

typedef enum mbr_partition_type mbr_partition_type_t;

enum mbr_partition_type {
    MBR_PARTITION_TYPE_EMPTY          = 0x00,
    MBR_PARTITION_TYPE_FAT12          = 0x01,
    MBR_PARTITION_TYPE_FAT16_SMALL    = 0x04,
    MBR_PARTITION_TYPE_EXTENDED       = 0x05,
    MBR_PARTITION_TYPE_FAT16_LARGE    = 0x06,
    MBR_PARTITION_TYPE_NTFS           = 0x07, // Also exFAT
    MBR_PARTITION_TYPE_FAT32          = 0x0B,
    MBR_PARTITION_TYPE_FAT32_LBA      = 0x0C,
    MBR_PARTITION_TYPE_NTFS_LBA       = 0x0E,
    MBR_PARTITION_TYPE_EXTENDED_LBA   = 0x0F,
    MBR_PARTITION_TYPE_LINUX_SWAP     = 0x82,
    MBR_PARTITION_TYPE_LINUX_NATIVE   = 0x83,
    MBR_PARTITION_TYPE_LINUX_LVM      = 0x8E,
    MBR_PARTITION_TYPE_APPLE_HFS      = 0xAF,
    MBR_PARTITION_TYPE_EFI_SYSTEM     = 0xEF,
    MBR_PARTITION_TYPE_BIOS_BOOT      = 0xEF, // Same as EFI System Partition
    MBR_PARTITION_TYPE_UNKNOWN        = 0xFF,
};

struct mbr_partition_entry {
    union {
        uint8_t boot_indicator;    // 0x80 = bootable, 0x00 = non-bootable and non-existent
        struct {
            uint8_t bootable : 1;
            uint8_t : 7;
        } __attribute__((packed));
    } __attribute__((packed));

    /**
     * The next 3 bytes represent the start of the partition in legacy CHS format.
     */
    uint8_t starting_head;     // Bits 0-7: head
    uint8_t starting_sector;   // Bits 0-5: sector (1-63), Bits 6-7: high bits of cylinder
    uint8_t starting_cylinder; // Bits 0-7: low bits of cylinder
    
    mbr_partition_type_t partition_type : 8;    // See https://en.wikipedia.org/wiki/Partition_type

    /**
     * The next 3 bytes represent the end of the partition in legacy CHS format.
     */
    uint8_t ending_head;
    uint8_t ending_sector; // Bits 0-5: sector (1-63), Bits 6-7: high bits of cylinder
    uint8_t ending_cylinder;

    /**
     * The next 8 bytes represent the starting LBA and size of the partition.
     */
    uint32_t starting_lba;    // Starting LBA of the partition
    uint32_t size_in_sectors; // Size of the partition in sectors
} __attribute__((packed));

struct MBR {
    uint8_t boot_code[440];
    uint8_t disk_signature[4]; // Optional; boot_code can expand into this space
    uint16_t reserved;         // Optional; boot_code can expand into this space

    struct mbr_partition_entry partitions[4];

    uint16_t signature; // Should be 0xAA55
} __attribute__((packed));

void drive_read_mbr(struct DRIVE *drive, struct MBR *mbr_out);