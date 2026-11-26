#include "mbr.h"

#include "drive.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "vfs/fat/exfat.h"
#include "vfs/fat/fat.h"
#include "vfs/fat/fat12.h"
#include "vfs/fat/fat16.h"
#include "vfs/fat/fat32.h"
#include "vfs/fscache.h"
#include "vfs/fscommon.h"
#include "vfs/iso9660/iso9660.h"

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

static fixed_size_allocator_t _mbr_part_fsa = FSA_DEFAULT(sizeof(partition_t));

// Note: MBR lba values are defined in 512-byte sectors, regardless of the drive's logical sector size
// Another note: do NOT expect drive to have an endpoint or name yet (but read sectors will be populated)
void drive_read_mbr(struct DRIVE *drive, struct MBR *mbr_out) {
    if (!drive) {
        kout(KERNEL_PANIC, "Kernel Programming Error: drive_read_mbr called with NULL drive\n");
        return;
    }

    if (!drive->read_sectors) {
        kout(KERNEL_EXTERNAL_FAULT, "Error: drive does not support reading sectors\n");
        return;
    }

    if (drive->logical_sector_size < sizeof(struct MBR)) {
        kout(KERNEL_EXTERNAL_FAULT,
             "Kernel Programming Error: drive_read_mbr(): drive logical sector size (%x) is smaller than sizeof(struct "
             "MBR) (%x)\n",
             drive->logical_sector_size, (uint32_t)sizeof(struct MBR));
        return;
    }

    uint8_t *full_buf = kmalloc_heap(drive->logical_sector_size);
    if (!full_buf) {
        kout(KERNEL_SEVERE_FAULT, "OOM Error: could not allocate memory for MBR read buffer\n");
        return;
    }

    // MBR is located in the first sector (LBA 0)
    ssize_t res = drive->read_sectors(drive, 0, 1, full_buf);
    if (res < 0) {
        kout(KERNEL_EXTERNAL_FAULT, "Error reading MBR from drive\n");
        kfree_heap(full_buf);
        return;
    }

    if (res < (ssize_t)sizeof(struct MBR)) {
        kout(KERNEL_EXTERNAL_FAULT, "Error: Incomplete MBR read from drive (expected %u bytes, got %d bytes)\n",
             sizeof(struct MBR), res);
        kfree_heap(full_buf);
        return;
    }

    if (mbr_out) {
        memcpy(mbr_out, full_buf, sizeof(struct MBR));
    }
    struct MBR *mbr = (struct MBR *)full_buf;

    kout(KERNEL_INFO, "MBR read successfully from drive\n");
    kout(KERNEL_INFO, "MBR Signature: %x; disk_signature: %x\n", (uint32_t)mbr->signature,
         *(uint32_t *)mbr->disk_signature);
    kout(KERNEL_INFO, "Partitions:\n");
    // No need to set partition count; the drive_t driver does it when creating endpoints
    // drive->partition_count = 4;
    for (int i = 0; i < 4; i++) {
        struct mbr_partition_entry *part = &mbr->partitions[i];

        uint32_t lba_act = part->starting_lba * 512 / drive->logical_sector_size;

        if (part->partition_type != 0x00) {
            kout(KERNEL_INFO, "  Partition %d: Type %s (%x), Bootable: %s, Start LBA: %u, Size (sectors): %u\n", i + 1,
                 mbr_pretty_partition_type(part->partition_type), part->partition_type,
                 (part->boot_indicator == 0x80) ? "Yes" : "No", lba_act, part->size_in_sectors);
        }

        partition_t *drive_part = fsa_alloc(&_mbr_part_fsa);
        if (!drive_part) {
            partition_t *cur = drive->first_partition;
            while (cur) {
                partition_t *next = cur->next;
                fsa_free(&_mbr_part_fsa, (void *)cur);
                cur = next;
            }
            kout(KERNEL_WARN, "drive_read_mbr(): fsa_alloc() failed (OOM?)\n");
            break;
        }

        if (drive->first_partition) {
            partition_t *cur = drive->first_partition;
            while (cur->next) {
                cur = cur->next;
            }
            cur->next = drive_part;
        } else {
            drive->first_partition = drive_part;
        }

        drive_part->drive          = drive;
        drive_part->partition_type = part->partition_type;
        drive_part->lba_act        = lba_act;
        drive_part->next           = NULL;
    }

    kfree_heap(full_buf);
}

// Partition no. starts at 1, NOT 0
// drive MUST have its endpoint populated
int mbr_mount(struct DRIVE *drive, int partition_no, uid_t uid, const char *path) {
    fscache_node_t *node = NULL;
    int res              = fscache_request_node_for_mounting(path, uid, &node);
    if (res) {
        // node will still be NULL; no releasing needed
        kout(KERNEL_SEVERE_EXTERNAL_FAULT, "mbr_mount(): fscache_request_node_for_mounting() failed with res %d\n", res);
        return res;
    }

    // TODO: IMPORTANT: restore proper fsops if mount fails

    partition_t *part = drive->first_partition;
    for (int i = 1; i < partition_no; i++) {
        if (part) {
            part = part->next;
        }
    }

    if (!part) {
        kout(KERNEL_SEVERE_EXTERNAL_FAULT, "mbr_mount(): invalid partition no %d for drive %s\n", partition_no,
             drive->endpoint->name);
        return -EINVAL;
    }

    uint8_t *buf = kmalloc_heap(drive->logical_sector_size);
    if (!buf) {
        kout(KERNEL_SEVERE_EXTERNAL_FAULT, "mbr_mount(): kmalloc_heap() failed (OOM?)\n");
        return -ENOMEM;
    }

    res = read_first_sector(drive, part->lba_act, buf);
    if (res) {
        kout(KERNEL_SEVERE_EXTERNAL_FAULT, "Error reading first sector of partition %d (errno %d)\n", partition_no,
             res);
        res = -EIO; // TODO: should we just return res instead?
        goto cleanup;
    }

    res = 0;

    if (part->partition_type == MBR_PARTITION_TYPE_EMPTY) {
        kout(KERNEL_INFO, "mbr_mount(): partition type is empty; cannot mount.\n");
        res = -EINVAL;
        goto cleanup;
    }

    if (part->lba_act == 16) {
        // Potential ISO9660 partition (e.g., Live CD)
        if (memcmp(buf + 1, "CD001", 5) == 0) {
            kout(KERNEL_INFO, "    ISO9660 filesystem detected on partition %d\n", partition_no);

            if (buf[0] != 0x01) {
                kout(KERNEL_WARN, "    Warning: Primary Volume Descriptor type is not 0x01 (got %02x)\n", buf[0]);
                res = -EIO; // TODO: is this correct?
            }

            struct filesystem_iso9660 *fs = kmalloc_heap(sizeof(struct filesystem_iso9660));
            if (!fs) {
                kout(KERNEL_SEVERE_EXTERNAL_FAULT,
                     "OOM Error: could not allocate memory for ISO9660 filesystem structure\n");
                res = -ENOMEM;
            }
            memset(fs, 0, sizeof(struct filesystem_iso9660));
            memcpy(&fs->pvd, buf, sizeof(struct iso9660_primary_volume_descriptor));
            fs->read_status |= 0x01; // Mark primary volume descriptor as read
            res              = iso9660_setup(fs, drive, part, 0, node);
            if (res == 0) {
                kout(KERNEL_INFO, "    ISO9660 setup successful on partition %d\n", partition_no);
            } else {
                kout(KERNEL_SEVERE_EXTERNAL_FAULT, "    Failed to setup ISO9660 filesystem on partition %d (res %d)\n",
                     partition_no, res);
                goto cleanup;
            }
        }

        goto cleanup;
    }

    switch (part->partition_type) {
    case MBR_PARTITION_TYPE_FAT32:
    case MBR_PARTITION_TYPE_FAT32_LBA: {
        struct filesystem_fat32 *fs = kmalloc_heap(sizeof(struct filesystem_fat32));
        if (!fs) {
            res = -ENOMEM;
            kout(KERNEL_WARN, "mbr_mount(): fat32: kmalloc_heap() failed (OOM?)\n");
            break;
        }
        memcpy(&fs->boot_sector_raw, (struct fat_boot_sector *)buf, sizeof(struct fat_boot_sector));
        kout(KERNEL_INFO, "    FAT32 boot sector read successfully on partition %d\n", +1);
        res = fat32_setup(fs, part);
        if (res == 0) {
            kout(KERNEL_INFO, "    FAT32 filesystem detected on partition %d: Total clusters: %u\n", partition_no,
                 fs->total_clusters);
            // Parse and list root directory for basic validation
            // fat32_parse_root(fs);
            fat32_mount(fs, node);
        } else {
            kout(KERNEL_SEVERE_EXTERNAL_FAULT, "    Failed to setup FAT32 filesystem on partition %d\n", partition_no);
        }

        break;
    }
    /* case MBR_PARTITION_TYPE_FAT16_SMALL:
    case MBR_PARTITION_TYPE_FAT16_LARGE: {
        break;
    }
    case MBR_PARTITION_TYPE_FAT12: {
        break;
    }
    case MBR_PARTITION_TYPE_EFI_SYSTEM: {
        // Can be FAT12, FAT16, or FAT32
        fat_type_t fat_type = fat_detect_type(drive, part->lba_act, (struct fat_boot_sector *)buf);

        switch (fat_type) {
        case FAT_TYPE_12: {
            kout(KERNEL_INFO, "    FAT12 filesystem detected on EFI System Partition %d\n", partition_no);
            struct filesystem_fat12 *fs = kmalloc_heap(sizeof(struct filesystem_fat12));
            memcpy(&fs->boot_sector_raw, (struct fat_boot_sector *)buf, sizeof(struct fat_boot_sector));
            fs->read_status |= 0x01; // Mark boot sector as read
            fat12_setup(fs, drive, part->lba_act);
            break;
        }
        case FAT_TYPE_16: {
            kout(KERNEL_INFO, "    FAT16 filesystem detected on EFI System Partition %d\n", partition_no);
            struct filesystem_fat16 *fs = kmalloc_heap(sizeof(struct filesystem_fat16));
            memcpy(&fs->boot_sector_raw, (struct fat_boot_sector *)buf, sizeof(struct fat_boot_sector));
            fs->read_status |= 0x01; // Mark boot sector as read
            fat16_setup(fs, drive, part->lba_act);
            break;
        }
        case FAT_TYPE_32: {
            kout(KERNEL_INFO, "    FAT32 filesystem detected on EFI System Partition %d\n", partition_no);
            struct filesystem_fat32 *fs = kmalloc_heap(sizeof(struct filesystem_fat32));
            memcpy(&fs->boot_sector_raw, (struct fat_boot_sector *)buf, sizeof(struct fat_boot_sector));
            fs->read_status |= 0x01; // Mark boot sector as read
            if (fat32_setup(fs, part) == 0) {
                // fat32_parse_root(fs);
            }
            break;
        }
        default: {
            kout(KERNEL_INFO, "    Unknown or unsupported filesystem on EFI System Partition %d\n", partition_no);
            break;
        }
        }
        break;
    }
    case MBR_PARTITION_TYPE_NTFS: {
        // NTFS or exFAT
        char *signature = (buf + 3);
        if (memcmp(signature, "EXFAT   ", 8) == 0) {
            kout(KERNEL_INFO, "    exFAT filesystem detected on partition %d\n", partition_no);

            struct filesystem_exfat *fs = kmalloc_heap(sizeof(struct filesystem_exfat));
            memcpy(&fs->boot_sector, buf, sizeof(struct exfat_boot_sector));
            fs->read_status |= 0x01; // Mark boot sector as read
            int res          = exfat_setup(fs, drive, part->lba_act);
            if (res == 0) {
                kout(KERNEL_INFO, "    exFAT setup successful on partition %d: Total clusters: %u\n", partition_no,
                     fs->total_clusters);
            } else {
                kout(KERNEL_SEVERE_EXTERNAL_FAULT, "    Failed to setup exFAT filesystem on partition %d\n",
                     partition_no);
            }
        } else if (memcmp(signature, "NTFS    ", 8) == 0) {
            kout(KERNEL_INFO, "    NTFS filesystem detected on partition %d\n", partition_no);
        } else {
            kout(KERNEL_SEVERE_EXTERNAL_FAULT,
                 "    Unknown filesystem signature (%.8s, jmp %02x %02x %02x) on NTFS partition %d\n", signature,
                 (uint8_t)buf[0], (uint8_t)buf[1], (uint8_t)buf[2], partition_no);
        }
        break;
    } */
    default: {
        res = -ENOSYS; // TODO: should this be -ENODEV instead?
        break;
    }
    }

cleanup:
    if (res) {
        fscache_node_unref(node);
    }
    _fscache_release_node_modifiable(node);
    kfree_heap(buf);
    return res;
}