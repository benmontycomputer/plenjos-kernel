#pragma once

#include "devices/storage/drive.h"

#include <stddef.h>
#include <stdint.h>

// https://wiki.osdev.org/ExFAT

/**
 * Structure of exFAT partition structures:
 *
 * Main Boot Region:
 *  Offset 0: boot sector (1 sector)
 *  Offset 1: extended boot sectors (8 sectors)
 *  Offset 9: OEM parameters sector
 *  Offset 10: reserved sector
 *  Offset 11: boot checksum sector
 *
 * Backup Boot Region:
 *  Offset 12: backup of main boot region (12 sectors)
 *
 * FAT Region:
 *  Offset 24: FAT alignment ((FATOffset - 24) sectors)
 *  Offset FATOffset: FAT area (FATLength sectors)
 *  Offset FATOffset + FATLength: Second FAT area (FATLength sectors)
 *  etc.
 *
 * Data Region: (DataOffset = FATOffset + (NumberOfFATs * FATLength))
 *  Offset DataOffset: Cluster heap alignment (DataLength sectors)
 *  Offset (variable): Cluster heap
 *  End: Excess space
 *
 * FAT alignment and Cluster heap alignment appear to be filler to ensure that the FAT tables and the Cluster heap are
 * nicely aligned.
 */

/**
 * Boot sector struct
 */
struct exfat_boot_sector {
    uint8_t JumpBoot[3];
    uint8_t FileSystemName[8];
    uint8_t MustBeZero[53];               // Prevents FAT drivers from accidentally parsing an exFAT partition
    uint64_t PartitionOffset;             // Offset from start of volume to start of partition in sectors
    uint64_t VolumeLength;                // Total size of this partition in sectors (should be >=1MiB)
    uint32_t FatOffset;                   // Offset from partition start to first FAT in sectors
    uint32_t FatLength;                   // Size of each FAT in sectors
    uint32_t ClusterHeapOffset;           // Offset from partition start to cluster heap in sectors
    uint32_t ClusterCount;                // Total number of clusters in the cluster heap
    uint32_t FirstClusterOfRootDirectory; // First cluster of the root directory
    uint32_t VolumeSerialNumber;
    uint16_t FileSystemRevision; // Major.Minor (e.g., 1.0 = 0x0100)

    /**
     * NOTE: this word is not included in checksumming.
     *
     * Bit 0: ActiveFAT - Used to implement transaction-safe mechanisms
     *   - 0 = FAT 0 is active
     *   - 1 = FAT 1 is active
     * Bit 1: VolumeDirty - Indicates whether the volume was unmounted cleanly
     *   - 0 = Cleanly unmounted
     *   - 1 = Not cleanly unmounted
     * Bit 2: MediaFailure - Indicates whether a media failure has been detected
     * Bit 3: ClearToZero - No real meaning (yet!)
     * Bits 4-15: Reserved
     */
    uint16_t VolumeFlags;

    uint8_t BytesPerSectorShift;    // Bytes per sector = 2^value (valid range: 9-12 for 512-4096 bytes)
    uint8_t SectorsPerClusterShift; // Sectors per cluster = 2^value (valid range: 0-7 for 1-128 sectors)
    uint8_t NumberOfFats;           // Number of FATs (usually 2)
    uint8_t DriveSelect;            // BIOS drive number (e.g., 0x80 for first hard disk)

    /* NOTE: this byte is not included in checksumming. */
    uint8_t PercentInUse; // Percentage of cluster heap in use (rounded down)

    uint8_t Reserved[7];
    uint8_t BootCode[390];
    uint16_t BootSectorSignature; // Should be 0xAA55
} __attribute__((packed));

struct filesystem_exfat {
    DRIVE_t *drive;
    uint32_t partition_start_lba;

    uint8_t read_status; // Bitfield: 0x01 = boot sector read

    struct exfat_boot_sector boot_sector;

    uint32_t fat_size_sectors;
    uint32_t cluster_heap_start_lba; // Relative to partition start; uses drive sector size
    uint32_t total_sectors;
    uint32_t total_clusters;
};

int exfat_setup(struct filesystem_exfat *fs, DRIVE_t *drive, uint32_t partition_start_lba);