#include "fat32.h"

#include "lib/stdio.h"
#include "memory/kmalloc.h"

int fat32_setup(struct filesystem_fat32 *fs, DRIVE_t *drive, uint32_t partition_start_lba) {
    if (!fs || !drive) {
        return -1;
    }

    fs->drive = drive;
    if (!(fs->read_status & 0x01)) {
        // Read boot sector from the partition start LBA
        uint8_t *bs_buf = kmalloc_heap(drive->logical_sector_size);
        if (!bs_buf) {
            printf("FAT32: OOM reading boot sector\n");
            return -1;
        }

        ssize_t r = drive->read_sectors(drive, partition_start_lba, 1, bs_buf);
        if (r < 0) {
            printf("FAT32: Failed to read boot sector (LBA %u)\n", partition_start_lba);
            kfree_heap(bs_buf);
            return -1;
        }

        // Copy into the raw boot sector structure
        memcpy(&fs->boot_sector_raw, bs_buf, sizeof(struct fat_boot_sector));
        kfree_heap(bs_buf);

        // Verify this looks like FAT32
        fat_type_t detected = fat_detect_type(drive, partition_start_lba, &fs->boot_sector_raw);
        if (detected != FAT_TYPE_32) {
            printf("Error: not a FAT32 filesystem (detected type %d)\n", detected);
            return -1;
        }

        fs->read_status |= 0x01; // Mark boot sector as read
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

    // Calculate factor between drive sector size and FAT sector size
    fs->factor = 1;
    if (fs->boot_sector.generic.bytes_per_sector != fs->drive->logical_sector_size) {
        fs->factor = fs->drive->logical_sector_size / fs->boot_sector.generic.bytes_per_sector;
    }

    printf("FAT32: Total clusters: %u\n", fs->total_clusters);
    printf("FAT32: Free clusters (from FSInfo): %u\n", fs->fsinfo.free_cluster_count);
    printf("FAT32: Next free cluster (from FSInfo): %u\n", fs->fsinfo.next_free_cluster);

    return 0;
}

// Read FAT or cluster data, translating from FAT sector size to drive sector size
int fat32_drive_read(struct filesystem_fat32 *fs, uint32_t fat_lba, uint32_t fat_sectors, void *buffer, uint32_t bytes_to_read) {
    if (!fs || !buffer || fat_sectors == 0 || bytes_to_read == 0) {
        return -1;
    }

    struct fat_boot_sector_generic *bpb = &fs->boot_sector.generic;
    uint32_t fat_bytes_per_sector       = bpb->bytes_per_sector;

    if (fs->factor == 1) {
        // 1:1 mapping; direct read
        return fs->drive->read_sectors(fs->drive, fat_lba, fat_sectors, buffer);
    }

    uint8_t *tmp_buffer = kmalloc_heap(fs->drive->logical_sector_size);
    if (!tmp_buffer) {
        printf("OOM Error in fat32_drive_read\n");
        return -1;
    }

    uint32_t remaining_fat_sectors = fat_sectors;
    uint32_t current_fat_lba       = fat_lba;
    uint8_t *out_ptr               = (uint8_t *)buffer;
    uint32_t bytes_read            = 0;

    while (bytes_read < bytes_to_read && remaining_fat_sectors > 0) {
        uint32_t drive_lba = fs->partition_start_lba + current_fat_lba / fs->factor;
        uint32_t offset    = current_fat_lba % fs->factor;

        // Number of FAT sectors we can read in this iteration
        uint32_t sectors_this_iter = fs->factor - offset;
        if (sectors_this_iter > remaining_fat_sectors) {
            sectors_this_iter = remaining_fat_sectors;
        }

        // Calculate bytes to read in this iteration
        uint32_t bytes_this_iter = sectors_this_iter * fat_bytes_per_sector;
        uint32_t bytes_to_read_this_iter = bytes_this_iter < (bytes_to_read - bytes_read)
                                          ? bytes_this_iter
                                          : (bytes_to_read - bytes_read);

        // Read one drive sector
        int ret = fs->drive->read_sectors(fs->drive, drive_lba, 1, tmp_buffer);
        if (ret < 0) {
            kfree_heap(tmp_buffer);
            return -1;
        }

        // Copy relevant FAT sectors to output buffer
        memcpy(out_ptr, (uint8_t *)tmp_buffer + (offset * fat_bytes_per_sector),
               bytes_to_read_this_iter);

        out_ptr               += bytes_to_read_this_iter;
        bytes_read            += bytes_to_read_this_iter;
        remaining_fat_sectors -= sectors_this_iter;
        current_fat_lba       += sectors_this_iter;
    }

    kfree_heap(tmp_buffer);
    return (bytes_read == bytes_to_read) ? 0 : -1;
}

// Reads a cluster's worth of data into buffer
int fat32_read_entry(struct filesystem_fat32 *fs, uint32_t cluster, void *buffer, uint32_t bytes_to_read) {
    if (!fs || cluster < 2 || !buffer) {
        return -1; // EOC or invalid
    }

    uint32_t sector = fs->cluster_heap_start_lba + (cluster - 2) * fs->boot_sector.generic.sectors_per_cluster;
    return fat32_drive_read(fs, sector, fs->boot_sector.generic.sectors_per_cluster, buffer, bytes_to_read);
}

int fat32_next_cluster(struct filesystem_fat32 *fs, uint32_t cluster, uint32_t *next) {
    if (!fs || !next || cluster < 2) {
        return -1; // EOC or invalid
    }

    uint32_t fat_offset = cluster * 4;

    uint8_t *sector_buf = kmalloc_heap(fs->boot_sector.generic.bytes_per_sector);
    if (!sector_buf) {
        printf("OOM Error in fat32_next_cluster\n");
        return -1;
    }
    uint32_t fat_sector       = fs->fat_start_lba + (fat_offset / fs->boot_sector.generic.bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % fs->boot_sector.generic.bytes_per_sector;

    if (fat32_drive_read(fs, fat_sector, 1, sector_buf, fs->boot_sector.generic.bytes_per_sector) < 0) {
        kfree_heap(sector_buf);
        return -1;
    }

    uint32_t entry = *((uint32_t *)&sector_buf[offset_in_sector]);
    kfree_heap(sector_buf);

    // Mask to 28 bits as per FAT32 spec
    entry &= 0x0FFFFFFF;

    *next = entry;

    return 0;

    return 0;
}

int fat32_parse_root(struct filesystem_fat32 *fs) {
    if (!fs) {
        return -1;
    }

    uint32_t cluster = fs->boot_sector.root_cluster;
    uint32_t bytes_per_cluster = fs->boot_sector.generic.bytes_per_sector * fs->boot_sector.generic.sectors_per_cluster;

    uint8_t *cluster_buf = kmalloc_heap(bytes_per_cluster);
    if (!cluster_buf) {
        return -1;
    }

    printf("FAT32 Root Directory (start cluster %u):\n", cluster);

    while (cluster < 0x0FFFFFF8) {
        if (fat32_read_entry(fs, cluster, cluster_buf, bytes_per_cluster) < 0) {
            kfree_heap(cluster_buf);
            return -1;
        }

        // Iterate directory entries in this cluster
        for (uint32_t off = 0; off + 32 <= bytes_per_cluster; off += 32) {
            struct fat32_directory_entry *entry = (struct fat32_directory_entry *)(cluster_buf + off);

            if ((uint8_t)entry->name[0] == 0x00) {
                // End of directory
                kfree_heap(cluster_buf);
                return 0;
            }
            if ((uint8_t)entry->name[0] == 0xE5) {
                // Deleted entry
                continue;
            }
            if (entry->attr == FAT_ATTR_LONG_NAME) {
                // Long name entry; skip for now
                continue;
            }

            char name[12];
            memcpy(name, entry->name, 11);
            name[11] = '\0';

            printf("Name: %.11s Attr: 0x%.2x Size: %u\n", name, entry->attr, entry->file_size);
        }

        uint32_t next;
        if (fat32_next_cluster(fs, cluster, &next) < 0) {
            break;
        }
        if (next == 0 || next >= 0x0FFFFFF8) {
            break;
        }
        cluster = next;
    }

    kfree_heap(cluster_buf);
    return 0;
}