#include "fat12.h"

#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "plenjos/errno.h"
#include "vfs/fscache.h"
#include "vfs/vfs.h"

// This function reads FAT sectors, translating from FAT sector size to drive sector size
int fat12_drive_read(struct filesystem_fat12 *fs, uint32_t fat_lba, uint32_t fat_sectors, void *buffer) {
    if (!fs || !buffer || fat_sectors == 0) {
        return -1;
    }

    struct fat_boot_sector_generic *bpb = &fs->boot_sector.generic;
    uint32_t fat_bytes_per_sector       = bpb->bytes_per_sector;

    if (fs->factor == 1) {
        // 1:1 mapping; direct read
        return fs->drive->read_sectors(fs->drive, fs->partition_start_lba + fat_lba, fat_sectors, buffer);
    }

    uint8_t *tmp_buffer = kmalloc_heap(fs->drive->logical_sector_size);
    if (!tmp_buffer) {
        printf("OOM Error in fat12_drive_read\n");
        return -ENOMEM;
    }

    uint32_t remaining_fat_sectors = fat_sectors;
    uint32_t current_fat_lba       = fat_lba;
    uint8_t *out_ptr               = (uint8_t *)buffer;

    while (remaining_fat_sectors > 0) {
        uint32_t drive_lba = fs->partition_start_lba + current_fat_lba / fs->factor;
        uint32_t offset    = current_fat_lba % fs->factor;

        // Number of FAT sectors we can read in this iteration
        uint32_t sectors_this_iter = fs->factor - offset;
        if (sectors_this_iter > remaining_fat_sectors) {
            sectors_this_iter = remaining_fat_sectors;
        }

        // Read one drive sector
        int ret = fs->drive->read_sectors(fs->drive, drive_lba, 1, tmp_buffer);
        if (ret < 0) {
            kfree_heap(tmp_buffer);
            return -1;
        }

        // Copy relevant FAT sectors to output buffer
        memcpy(out_ptr, (uint8_t *)tmp_buffer + (offset * fat_bytes_per_sector),
               sectors_this_iter * fat_bytes_per_sector);

        out_ptr               += sectors_this_iter * fat_bytes_per_sector;
        remaining_fat_sectors -= sectors_this_iter;
        current_fat_lba       += sectors_this_iter;
    }

    kfree_heap(tmp_buffer);
    return 0;
}

// Reads a cluster's worth of data into buffer
int fat12_read_entry(struct filesystem_fat12 *fs, uint16_t cluster, void *buffer) {
    if (!fs || cluster < 2) {
        return -1; // EOC
    }

    uint32_t sector = fs->cluster_heap_start_lba + (cluster - 2) * fs->boot_sector.generic.sectors_per_cluster;
    return fat12_drive_read(fs, sector, fs->boot_sector.generic.sectors_per_cluster, buffer);
}

int fat12_next_cluster(struct filesystem_fat12 *fs, uint16_t cluster, uint16_t *next) {
    if (!fs || !next || cluster < 2) {
        return -1; // EOC
    }

    uint32_t fat_offset = (cluster * 3) / 2;

    uint8_t *sector_buf = kmalloc_heap(fs->boot_sector.generic.bytes_per_sector);
    ;
    if (!sector_buf) {
        printf("OOM Error in fat12_next_cluster\n");
        return -ENOMEM;
    }
    uint32_t fat_sector       = fs->fat_start_lba + (fat_offset / fs->boot_sector.generic.bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % fs->boot_sector.generic.bytes_per_sector;

    if (fat12_drive_read(fs, fat_sector, 1, sector_buf) < 0) {
        goto return_error;
    }

    uint16_t entry;
    if (offset_in_sector == fs->boot_sector.generic.bytes_per_sector - 1) {
        // Need to read next byte from next sector
        entry = sector_buf[offset_in_sector];
        if (fat12_drive_read(fs, fat_sector + 1, 1, sector_buf) < 0) {
            goto return_error;
        }
        entry |= ((uint16_t)sector_buf[0]) << 8;
    } else {
        // Both bytes are in the same sector
        entry = *((uint16_t *)&sector_buf[offset_in_sector]);
    }

    kfree_heap(sector_buf);

    if (cluster & 0x0001) {
        // Odd cluster
        entry >>= 4;
    } else {
        // Even cluster
        entry &= 0x0FFF;
    }

    *next = entry;

    return 0;

return_error:
    kfree_heap(sector_buf);
    return -1;
}

int fat12_read_root_entry(struct filesystem_fat12 *fs, uint32_t entry_index, struct fat16_directory_entry *entry) {
    if (!fs || !entry) {
        return -1;
    }
    if (entry_index >= fs->boot_sector.generic.root_entry_count) {
        return -1;
    }

    uint32_t sector = fs->root_dir_start_lba + (entry_index * 32) / fs->boot_sector.generic.bytes_per_sector;
    uint8_t *buf    = kmalloc_heap(fs->boot_sector.generic.bytes_per_sector);
    if (!buf) {
        printf("OOM Error in fat12_read_root_entry\n");
        return -ENOMEM;
    }

    if (fat12_drive_read(fs, sector, 1, buf) < 0) {
        kfree_heap(buf);
        return -1;
    }

    memcpy(entry, buf + (entry_index * 32) % fs->boot_sector.generic.bytes_per_sector, sizeof(*entry));
    kfree_heap(buf);

    return 0;
}

int fat12_parse_root(struct filesystem_fat12 *fs) {
    if (!fs) {
        return -1;
    }

    uint32_t total_entries = fs->boot_sector.generic.root_entry_count;

    printf("FAT12 Root Directory Entries (count %u):\n", total_entries);

    for (uint32_t i = 0; i < total_entries; i++) {
        struct fat16_directory_entry entry;
        if (fat12_read_root_entry(fs, i, &entry) < 0) {
            printf("Error reading root directory entry %u\n", i);
            return -1;
        }

        if (entry.name[0] == 0x00) {
            // No more entries
            printf("End of root directory entries (%u).\n", i);
            break;
        }
        if ((uint8_t)entry.name[0] == 0xE5) {
            // Deleted entry
            continue;
        }
        if (entry.attr == FAT_ATTR_LONG_NAME) {
            // Long name entry; skip for now
            printf("Entry %u: Long name entry; skipping.\n", i);
            continue;
        }

        char name[12];
        memcpy(name, entry.name, 11);
        name[11] = '\0';

        printf("Entry %u: Name: %.11s, Attr: 0x%.2x, Size: %u bytes\n", i, name, entry.attr, entry.file_size);
    }

    return 0;
}

// TODO: optimize this function to use instance_data->current_cluster
ssize_t fat12_file_read_func(vfs_handle_t *handle, void *buf, size_t len) {
    vfs_fat12_handle_instance_data_t *instance_data = (vfs_fat12_handle_instance_data_t *)handle->instance_data;

    if (!instance_data || !buf || len == 0 || !handle->backing_node || !handle->backing_node->internal_data) {
        return -EIO;
    }

    vfs_fat12_cache_node_data_t *node_data = (vfs_fat12_cache_node_data_t *)handle->backing_node->internal_data;

    struct filesystem_fat12 *fs = node_data->fs;
    if (!fs) {
        return -EIO;
    }

    if (node_data->start_cluster < 2) {
        return -EIO;
    }

    uint32_t bytes_per_cluster = fs->boot_sector.generic.bytes_per_sector * fs->boot_sector.generic.sectors_per_cluster;

    uint8_t *cluster_buf = kmalloc_heap(bytes_per_cluster);
    if (!cluster_buf) {
        return -ENOMEM;
    }

    uint8_t *out_ptr = (uint8_t *)buf;
    size_t remaining = len;

    uint16_t cluster         = (uint16_t)instance_data->current_cluster;
    uint32_t cluster_index   = (uint32_t)instance_data->cluster_index;
    size_t offset_in_cluster = instance_data->cluster_pos;

    while (remaining > 0 && cluster < FAT12_CLUSTER_EOC) {
        // Read current cluster if needed
        if (fat12_read_entry(fs, cluster, cluster_buf) < 0) {
            kfree_heap(cluster_buf);
            return -EIO;
        }

        size_t to_copy = bytes_per_cluster - offset_in_cluster;
        if (to_copy > remaining) {
            to_copy = remaining;
        }

        memcpy(out_ptr, cluster_buf + offset_in_cluster, to_copy);

        if (to_copy < bytes_per_cluster - offset_in_cluster) {
            // Stopped mid-cluster
            offset_in_cluster += to_copy;
            break;
        } else {
            // Move to next cluster
            offset_in_cluster = 0;
        }

        remaining              -= to_copy;
        out_ptr                += to_copy;

        // Move to next cluster if needed
        if (remaining > 0) {
            uint16_t next_cluster;
            if (fat12_next_cluster(fs, cluster, &next_cluster) < 0) {
                break;
            }
            if (next_cluster >= FAT12_CLUSTER_EOC) {
                cluster = FAT12_CLUSTER_EOC;
                cluster_index++;
                break;
            }
            cluster = next_cluster;
            cluster_index++;
        }
    }

    kfree_heap(cluster_buf);

    size_t bytes_read = len - remaining;
    instance_data->current_cluster = cluster;
    instance_data->cluster_pos     = offset_in_cluster + bytes_read % bytes_per_cluster;
    instance_data->cluster_index   = cluster_index;
    instance_data->seek_pos       += bytes_read;

    // To be implemented
    return -EIO;
}

ssize_t fat12_dir_read_func(vfs_handle_t *handle, void *buf, size_t len) {
    // To be implemented
    return -EIO;
}

ssize_t fat12_file_write_func(vfs_handle_t *handle, const void *buf, size_t len) {
    // To be implemented
    return -EIO;
}

off_t fat12_file_seek_func(vfs_handle_t *handle, off_t offset, int whence) {
    // To be implemented
    return -EIO;
}

off_t fat12_dir_seek_func(vfs_handle_t *handle, off_t offset, int whence) {
    // To be implemented
    return -EIO;
}

int fat12_handle_close_func(vfs_handle_t *handle) {
    // TODO: free anything else necessary
    kfree_heap(handle->instance_data);
    return 0;
}

int fat12_create_child_func(fscache_node_t *parent, const char *name, dirent_type_t type, uid_t uid, gid_t gid,
                            mode_t mode, fscache_node_t *node) {
    // To be implemented
    return -EIO;
}

int fat12_load_node_func(fscache_node_t *node, const char *name, fscache_node_t *out) {
    // To be implemented
    return -EIO;
}

bool fat12_vfs_mount_func(const char *device, fscache_node_t *mount_point) {
    // To be implemented
    return false;
}

int fat12_get_vfs_filesystem(struct filesystem_fat12 *fs, vfs_filesystem_t *out_fs) {
    if (!out_fs) {
        return -1;
    }

    out_fs->name  = "fat12";
    out_fs->mount = NULL; // To be implemented

    out_fs->instance_data[0] = (uintptr_t)fs;
    out_fs->instance_data[1] = 0;
    out_fs->instance_data[2] = 0;
    out_fs->instance_data[3] = 0;

    return 0;
}

int fat12_setup(struct filesystem_fat12 *fs, DRIVE_t *drive, uint32_t partition_start_lba) {
    if (!fs || !drive) {
        return -1;
    }

    fs->drive = drive;

    if (!(fs->read_status & 0x01)) {
        // Read boot sector and detect type
        fat_type_t res = fat_detect_type(drive, partition_start_lba, &fs->boot_sector_raw);
        if (res != FAT_TYPE_12) {
            printf("Error: not a FAT12 filesystem (type: %s)\n", res == FAT_TYPE_32   ? "FAT32"
                                                                 : res == FAT_TYPE_16 ? "FAT16"
                                                                                      : "UNKNOWN");
            return -1;
        }

        fs->read_status |= 0x01; // Mark boot sector as read
    }

    struct fat_boot_sector_generic *bsg = &fs->boot_sector.generic;

    if (bsg->fat_size_16 == 0) {
        // Not a FAT12 filesystem
        printf("Error: Not a FAT12 filesystem (fat_size_16 is 0)\n");
        return -1;
    }

    if (fs->boot_sector.boot_sector_signature != 0xAA55) {
        // Invalid boot sector signature
        printf("Error: Invalid FAT12 boot sector signature: %.4x\n", fs->boot_sector.boot_sector_signature);
        return -1;
    }

    /* if (bsg->bytes_per_sector != drive->logical_sector_size) {
        printf("Warning: FAT12 bytes per sector (%u) does not match drive logical sector size (%u)\n",
               bsg->bytes_per_sector, drive->logical_sector_size);
    } */

    fs->partition_start_lba = partition_start_lba;

    printf("FAT12 filesystem setup successful. Size: %u bytes\n", bsg->total_sectors_16 * bsg->bytes_per_sector);

    // Calculate factor between drive sector size and FAT sector size
    fs->factor = 1;
    if (bsg->bytes_per_sector != fs->drive->logical_sector_size) {
        fs->factor = fs->drive->logical_sector_size / bsg->bytes_per_sector;
    }

    // Calculate total sectors
    fs->total_sectors = (bsg->total_sectors_16 != 0) ? bsg->total_sectors_16 : bsg->total_sectors_32;

    // FAT and root directory sizes
    fs->fat_sectors      = bsg->fat_size_16;
    fs->root_dir_sectors = ((bsg->root_entry_count * 32) + (bsg->bytes_per_sector - 1)) / bsg->bytes_per_sector;

    // Layout (relative to partition start; using FAT sectors)
    fs->fat_start_lba          = bsg->reserved_sector_count;
    fs->root_dir_start_lba     = bsg->reserved_sector_count + (bsg->num_fats * fs->fat_sectors);
    fs->cluster_heap_start_lba = fs->root_dir_start_lba + fs->root_dir_sectors;

    // Calculate total clusters
    uint32_t data_sectors
        = fs->total_sectors - (bsg->reserved_sector_count + (bsg->num_fats * fs->fat_sectors) + fs->root_dir_sectors);
    fs->total_clusters = data_sectors / bsg->sectors_per_cluster;

    // Enforce FAT12 cluster count limits
    if (fs->total_clusters >= 4085) {
        printf("Error: Detected cluster count (%u) exceeds FAT12 limit\n", fs->total_clusters);
        return -1;
    }

    fat12_parse_root(fs);

    return 0;
}