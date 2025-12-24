#include "mbr.h"

#include "drive.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "vfs/fat/fat.h"
#include "vfs/fat/fat12.h"
#include "vfs/fat/fat16.h"
#include "vfs/fat/fat32.h"

static char *mbr_pretty_partition_type(mbr_partition_type_t type) {
    switch (type) {
    case MBR_PARTITION_TYPE_EMPTY:
        return "Empty";
    case MBR_PARTITION_TYPE_FAT12:
        return "FAT12";
    case MBR_PARTITION_TYPE_FAT16_SMALL:
        return "FAT16 Small";
    case MBR_PARTITION_TYPE_EXTENDED:
        return "Extended";
    case MBR_PARTITION_TYPE_FAT16_LARGE:
        return "FAT16 Large";
    case MBR_PARTITION_TYPE_NTFS:
        return "NTFS or exFAT";
    case MBR_PARTITION_TYPE_FAT32:
        return "FAT32";
    case MBR_PARTITION_TYPE_FAT32_LBA:
        return "FAT32 LBA";
    case MBR_PARTITION_TYPE_NTFS_LBA:
        return "NTFS LBA";
    case MBR_PARTITION_TYPE_EXTENDED_LBA:
        return "Extended LBA";
    case MBR_PARTITION_TYPE_LINUX_SWAP:
        return "Linux Swap";
    case MBR_PARTITION_TYPE_LINUX_NATIVE:
        return "Linux Native";
    case MBR_PARTITION_TYPE_LINUX_LVM:
        return "Linux LVM";
    case MBR_PARTITION_TYPE_APPLE_HFS:
        return "Apple HFS/HFS+";
    case MBR_PARTITION_TYPE_EFI_SYSTEM: // Also BIOS Boot Partition
        return "EFI System Partition / BIOS Boot Partition";
    default:
        return "Unknown";
    }
}

// Note: MBR lba values are defined in 512-byte sectors, regardless of the drive's logical sector size
void drive_read_mbr(struct DRIVE *drive, struct MBR *mbr_out) {
    if (!drive || !mbr_out) {
        printf("Kernel Programming Error: drive_read_mbr called with NULL drive or mbr_out\n");
        return;
    }

    if (!drive->read_sectors) {
        printf("Error: drive does not support reading sectors\n");
        return;
    }

    uint8_t *buf = kmalloc_heap(drive->logical_sector_size);
    if (!buf) {
        printf("OOM Error: could not allocate memory for MBR read buffer\n");
        return;
    }

    // MBR is located in the first sector (LBA 0)
    ssize_t res = drive->read_sectors(drive, 0, 1, buf);
    if (res < 0) {
        printf("Error reading MBR from drive\n");
        kfree_heap(buf);
        return;
    }

    if (res != sizeof(struct MBR)) {
        printf("Warning: Incomplete MBR read from drive (expected %u bytes, got %d bytes)\n", sizeof(struct MBR), res);
    }

    memcpy(mbr_out, buf, sizeof(struct MBR));
    kfree_heap(buf);

    printf("MBR read successfully from drive\n");
    printf("MBR Signature: %x\n", mbr_out->signature);
    printf("Partitions:\n");
    for (int i = 0; i < 4; i++) {
        struct mbr_partition_entry *part = &mbr_out->partitions[i];

        uint32_t lba_act = part->starting_lba * 512 / drive->logical_sector_size;

        if (part->partition_type != 0x00) {
            printf("  Partition %d: Type %s (%x), Bootable: %s, Start LBA: %u, Size (sectors): %u\n", i + 1,
                   mbr_pretty_partition_type(part->partition_type), part->partition_type,
                   (part->boot_indicator == 0x80) ? "Yes" : "No", lba_act, part->size_in_sectors);
        }

        switch (part->partition_type) {
        case MBR_PARTITION_TYPE_FAT32:
        case MBR_PARTITION_TYPE_FAT32_LBA: {
            struct filesystem_fat32 fs = { 0 };
            if (fat32_setup(&fs, drive, lba_act) == 0) {
                printf("    FAT32 filesystem detected on partition %d: Total clusters: %u\n", i + 1, fs.total_clusters);
            } else {
                printf("    Failed to setup FAT32 filesystem on partition %d\n", i + 1);
            }

            break;
        }
        case MBR_PARTITION_TYPE_FAT16_SMALL:
        case MBR_PARTITION_TYPE_FAT16_LARGE: {
            break;
        }
        case MBR_PARTITION_TYPE_FAT12: {
            break;
        }
        case MBR_PARTITION_TYPE_EFI_SYSTEM: {
            // Can be FAT12, FAT16, or FAT32
            struct fat_boot_sector bs;
            fat_type_t fat_type = fat_detect_type(drive, lba_act, &bs);

            switch (fat_type) {
            case FAT_TYPE_12: {
                printf("    FAT12 filesystem detected on EFI System Partition %d\n", i + 1);
                struct filesystem_fat12 fs = { 0 };
                memcpy(&fs.boot_sector, &bs, sizeof(struct fat_boot_sector));
                fs.read_status |= 0x01; // Mark boot sector as read
                fat12_setup(&fs, drive, lba_act);
                break;
            }
            case FAT_TYPE_16: {
                printf("    FAT16 filesystem detected on EFI System Partition %d\n", i + 1);
                struct filesystem_fat16 fs = { 0 };
                memcpy(&fs.boot_sector, &bs, sizeof(struct fat_boot_sector));
                fs.read_status |= 0x01; // Mark boot sector as read
                fat16_setup(&fs, drive, lba_act);
                break;
            }
            case FAT_TYPE_32: {
                printf("    FAT32 filesystem detected on EFI System Partition %d\n", i + 1);
                struct filesystem_fat32 fs = { 0 };
                memcpy(&fs.boot_sector, &bs, sizeof(struct fat_boot_sector));
                fs.read_status |= 0x01; // Mark boot sector as read
                fat32_setup(&fs, drive, lba_act);
                break;
            }
            default: {
                printf("    Unknown or unsupported filesystem on EFI System Partition %d\n", i + 1);
                break;
            }
            }
            break;
        }
        }
    }
}