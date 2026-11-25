#include "fat32.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "timer/timer.h"
#include "vfs/fscache.h"

// A pointer to this indicates that we should check the root directory of the filesystem (fat32 doesn't have an actual
// entry for the root directory)
const static struct fat32_directory_entry _fat32_placeholder_root = {
    .attr = FAT_ATTR_DIRECTORY,
};

int fat32_load_func(fscache_node_t *node, const char *name, fscache_node_t *out);
int fat32_unload_func(fscache_node_t *node);
ssize_t fat32_directory_read_func(vfs_handle_t *handle, void *buf, size_t len);
int fat32_create_child_func(fscache_node_t *parent, const char *name, struct kstat stat, fscache_node_t *node);
ssize_t fat32_file_read_func(vfs_handle_t *handle, void *buf, size_t len);

// TODO: seek
const vfs_ops_block_t fat32_file_fsops = {
    .read        = fat32_file_read_func,
    .load_node   = fat32_load_func,
    .unload_node = fat32_unload_func,
};
const vfs_ops_block_t fat32_directory_fsops = {
    .read         = fat32_directory_read_func,
    .load_node    = fat32_load_func,
    .create_child = fat32_create_child_func,
    .unload_node  = fat32_unload_func,
};

int fat32_is_long_entry(void *entry) {
    struct fat32_directory_entry *dir_entry = (struct fat32_directory_entry *)entry;
    return dir_entry->attr == FAT_ATTR_LONG_NAME;
}

int fat32_setup(struct filesystem_fat32 *fs, partition_t *partition) {
    if (!fs || !partition || !partition->drive) {
        return -1;
    }

    DRIVE_t *drive   = partition->drive;
    uint32_t lba_act = partition->lba_act;

    // TODO: maybe take these in as args?
    fs->mount_uid   = 0;
    fs->mount_gid   = 0;
    fs->mount_umask = 0;
    fs->exec        = true;

    fs->drive     = drive;
    fs->partition = partition;
    if (!(fs->read_status & 0x01)) {
        // Read boot sector from the partition start LBA
        uint8_t *bs_buf = kmalloc_heap(drive->logical_sector_size);
        if (!bs_buf) {
            printf("FAT32: OOM reading boot sector\n");
            return -1;
        }

        ssize_t r = drive->read_sectors(drive, lba_act, 1, bs_buf);
        if (r < 0) {
            printf("FAT32: Failed to read boot sector (LBA %u)\n", lba_act);
            kfree_heap(bs_buf);
            return -1;
        }

        // Copy into the raw boot sector structure
        memcpy(&fs->boot_sector_raw, bs_buf, sizeof(struct fat_boot_sector));
        kfree_heap(bs_buf);

        // Verify this looks like FAT32
        fat_type_t detected = fat_detect_type(drive, lba_act, &fs->boot_sector_raw);
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
            ssize_t res = drive->read_sectors(drive, (uint64_t)lba_act + (uint64_t)fs->boot_sector.fs_info, 1, buffer);
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
    fs->total_clusters    = data_sectors / fs->boot_sector.generic.sectors_per_cluster;

    fs->fat_start_lba          = lba_act + fs->boot_sector.generic.reserved_sector_count;
    fs->cluster_heap_start_lba = fs->fat_start_lba + (fs->boot_sector.generic.num_fats * fs->boot_sector.fat_size_32);
    fs->sectors_per_fat        = fs->boot_sector.fat_size_32;
    fs->total_sectors          = fs->boot_sector.generic.total_sectors_32;
    fs->partition_start_lba    = lba_act;

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
int fat32_drive_read(struct filesystem_fat32 *fs, uint32_t fat_lba, uint32_t fat_sectors, void *buffer,
                     uint32_t bytes_to_read) {
    if (!fs || !buffer || fat_sectors == 0 || bytes_to_read == 0) {
        kout(KERNEL_PANIC, "fat32_drive_read(): invalid args (%p %p %u %u)\n", fs, buffer, fat_sectors, bytes_to_read);
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
        kout(KERNEL_WARN, "fat32_drive_read(): kmalloc_heap() failed (OOM?)\n");
        return -ENOMEM;
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
        uint32_t bytes_to_read_this_iter
            = bytes_this_iter < (bytes_to_read - bytes_read) ? bytes_this_iter : (bytes_to_read - bytes_read);

        // Read one drive sector
        int ret = fs->drive->read_sectors(fs->drive, drive_lba, 1, tmp_buffer);
        if (ret < 0) {
            kfree_heap(tmp_buffer);
            kout(KERNEL_EXTERNAL_FAULT, "fat32_drive_read(): fs->drive->read_sectors() failed with errno %d.\n", ret);
            return -EIO;
        }

        // Copy relevant FAT sectors to output buffer
        memcpy(out_ptr, (uint8_t *)tmp_buffer + (offset * fat_bytes_per_sector), bytes_to_read_this_iter);

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
        kout(KERNEL_PANIC, "fat32_read_entry(): invalid args (%p %u %p %u)\n", fs, cluster, buffer, bytes_to_read);
        return -1; // EOC or invalid
    }

    uint32_t sector = fs->cluster_heap_start_lba + (cluster - 2) * fs->boot_sector.generic.sectors_per_cluster;
    return fat32_drive_read(fs, sector, fs->boot_sector.generic.sectors_per_cluster, buffer, bytes_to_read);
}

int fat32_next_cluster(struct filesystem_fat32 *fs, uint32_t cluster, uint32_t *next) {
    if (!fs || !next || cluster < 2) {
        kout(KERNEL_PANIC, "fat32_next_cluster(): invalid args (%p %u %p)\n", fs, cluster, next);
        return -1; // EOC or invalid
    }

    uint32_t fat_offset = cluster * 4;

    // Space for 2 sectors in case it crosses sector boundary (although idk if that's actually possible)
    uint8_t *sector_buf = kmalloc_heap(fs->boot_sector.generic.bytes_per_sector * 2);
    if (!sector_buf) {
        printf("OOM Error in fat32_next_cluster\n");
        return -1;
    }
    uint32_t fat_sector       = fs->fat_start_lba + (fat_offset / fs->boot_sector.generic.bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % fs->boot_sector.generic.bytes_per_sector;

    if (offset_in_sector <= fs->boot_sector.generic.bytes_per_sector - 4) {
        if (fat32_drive_read(fs, fat_sector, 1, sector_buf, fs->boot_sector.generic.bytes_per_sector) < 0) {
            kfree_heap(sector_buf);
            return -1;
        }
    } else {
        // Read 2 sectors
        if (fat32_drive_read(fs, fat_sector, 2, sector_buf, fs->boot_sector.generic.bytes_per_sector * 2) < 0) {
            kfree_heap(sector_buf);
            return -1;
        }
    }

    uint32_t entry = 0;
    memcpy(&entry, &sector_buf[offset_in_sector], sizeof(entry));
    kfree_heap(sector_buf);

    // Mask to 28 bits as per FAT32 spec
    entry &= 0x0FFFFFFF;

    *next = entry;

    return 0;
}

int fat32_mount(struct filesystem_fat32 *fs, fscache_node_t *mount_node) {
    vfs_fat32_cache_node_data_t *node_data = (vfs_fat32_cache_node_data_t *)mount_node->internal_data;

    memset(node_data, 0, sizeof(*node_data));
    node_data->dir_entry = &_fat32_placeholder_root;
    node_data->fs        = fs;
    mount_node->fsops    = &fat32_directory_fsops;

    return 0;
}

#define FAT32_CLUSTER_FROM_DIR_ENTRY(dir_entry)                                                      \
    ((uint32_t)0x0FFFFFFF                                                                            \
     & (((uint32_t)dir_entry->first_cluster_high << 16) | ((uint32_t)dir_entry->first_cluster_low)))

#define FAT32_END_OF_CHAIN 0x0FFFFFF8

static uint32_t fat32_dir_start_cluster(vfs_fat32_cache_node_data_t *node_data) {
    if (node_data->dir_entry == &_fat32_placeholder_root) {
        return node_data->fs->boot_sector.root_cluster;
    }
    return FAT32_CLUSTER_FROM_DIR_ENTRY(node_data->dir_entry);
}

struct fat32_lfn_state {
    uint16_t name16[FAT32_LFN_MAX_ORDER * 13]; // UTF-16
    uint8_t checksum;
    uint8_t next_order;
    uint8_t total;
};

/**
 * A fat32_dir_iter walks one directory's entries along the cluster chain. It has a 1-cluster buffer.
 * After fat32_dir_iter_next() returns an item, it->cluster and it->off hold the position to resume from.
 */
struct fat32_dir_iter {
    struct filesystem_fat32 *fs;
    uint8_t *buf;
    uint32_t bytes_per_cluster;

    // cluster and off are the scan's state; cluster is >= FAT32_END_OF_CHAIN when finished
    uint32_t cluster;
    uint32_t off;

    bool loaded; // whether buf currently holds `cluster`

    struct fat32_lfn_state *lfn_state;
};

struct fat32_dir_item {
    struct fat32_directory_entry *entry; // Points into the iterator's buffer; only valid until the next call
    char name[NAME_MAX + 1];             // Name to present to the VFS
};

static void fat32_dir_iter_drop_lfn(struct fat32_dir_iter *it) {
    it->lfn_state->checksum = 0;
    it->lfn_state->next_order = 0;
    it->lfn_state->total = 0;
}

static int fat32_dir_iter_init(struct fat32_dir_iter *it, struct filesystem_fat32 *fs, uint32_t cluster,
                               uint32_t pos_in_cluster) {
    it->fs                = fs;
    it->bytes_per_cluster = fs->boot_sector.generic.bytes_per_sector * fs->boot_sector.generic.sectors_per_cluster;
    it->cluster           = cluster;
    it->off               = pos_in_cluster;
    it->loaded            = false;
    it->buf               = kmalloc_heap(it->bytes_per_cluster);
    if (!it->buf) {
        kout(KERNEL_WARN, "fat32_dir_iter_init(): couldn't allocate cluster buffer (kmalloc_heap() failed) (OOM?)\n");
        return -ENOMEM;
    }
    it->lfn_state = kmalloc_heap(sizeof(*it->lfn_state));
    if (!it->lfn_state) {
        kout(KERNEL_WARN, "fat32_dir_iter_init(): couldn't allocate lfn state buffer (kmalloc_heap() failed) (OOM?)\n");
        kfree_heap(it->buf);
        return -ENOMEM;
    }
    fat32_dir_iter_drop_lfn(it);

    return 0;
}

static void fat32_dir_iter_free(struct fat32_dir_iter *it) {
    if (it->buf) {
        kfree_heap(it->buf);
        it->buf = NULL;
    }
    if (it->lfn_state) {
        kfree_heap(it->lfn_state);
        it->lfn_state = NULL;
    }
}

// Lowercase form of 8.3 entry
static void fat32_short_name(const struct fat32_directory_entry *entry, char *out) {
    int o = 0;
    for (int i = 0; i < 8; i++) {
        if (entry->name[i] != ' ') {
            out[o++] = tolower(entry->name[i]);
        }
    }

    int ext_start = o + 1;
    for (int i = 8; i < 11; i++) {
        if (entry->name[i] != ' ') {
            if (o < ext_start) {
                out[o++] = '.';
            }
            out[o++] = tolower(entry->name[i]);
        }
    }
    out[o] = '\0';
}

static uint8_t fat32_short_checksum(const char name[11]) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)name[i]);
    }
    return sum;
}

/**
 * Converts up to n UTF-16LE code units to NUL-terminated UTF-8, returning the number of bytes written excluding the
 * NULL, or -1 if `out` is too small.
 */
static int fat32_utf16_to_utf8(const uint16_t *in, size_t n, char *out, size_t out_size) {
    size_t o = 0;

    for (size_t i = 0; i < n && in[i] != 0; i++) {
        uint32_t cp = in[i];

        if (cp >= 0xD800 && cp <= 0xDBFF) {
            // High surrogate: needs a low surrogate right after it
            if (i + 1 < n && in[i + 1] >= 0xDC00 && in[i + 1] <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (in[i + 1] - 0xDC00);
                i++;
            } else {
                cp = 0xFFFD;
            }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            cp = 0xFFFD; // Lone low surrogate
        }

        size_t need = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (o + need + 1 > out_size) { // +1 keeps room for the NULL
            out[0] = '\0';
            return -1;
        }

        if (need == 1) {
            out[o++] = (char)cp;
        } else if (need == 2) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (need == 3) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }

    out[o] = '\0';
    return (int)o;
}

/**
 * Returns 1 and outputs item if there is another entry, 0 at the end of the directory, or -errno.
 */
static int fat32_dir_iter_next(struct fat32_dir_iter *it, struct fat32_dir_item *item) {
    while (it->cluster < FAT32_END_OF_CHAIN) {
        if (!it->loaded) {
            int res = fat32_read_entry(it->fs, it->cluster, it->buf, it->bytes_per_cluster);
            if (res < 0) {
                kout(KERNEL_WARN, "fat32_dir_iter_next(): fat32_read_entry() failed with errno %d\n", res);
                return -EIO;
            }
            it->loaded = true;
        }

        while (it->off + 32 <= it->bytes_per_cluster) {
            uint8_t *raw  = it->buf + it->off;
            it->off      += 32;

            if (raw[0] == 0x00) {
                // End of directory
                it->cluster = FAT32_END_OF_CHAIN;
                it->off     = 0;
                fat32_dir_iter_drop_lfn(it);
                return 0;
            }
            if (raw[0] == 0xE5) {
                // Deleted entry
                fat32_dir_iter_drop_lfn(it);
                continue;
            }

            if (fat32_is_long_entry(raw)) {
                struct fat32_long_directory_entry *entry = (struct fat32_long_directory_entry *)raw;

                uint8_t order = entry->order & 0x3F;
                if (order > FAT32_LFN_MAX_ORDER || order == 0) {
                    fat32_dir_iter_drop_lfn(it);
                    continue;
                }

                if (entry->order & 0x40) {
                    // Start of new LFN; reset state
                    fat32_dir_iter_drop_lfn(it);

                    it->lfn_state->checksum   = entry->checksum;
                    it->lfn_state->next_order = order;
                    it->lfn_state->total      = order;
                } else if (it->lfn_state->next_order != 0 && entry->order == it->lfn_state->next_order && entry->checksum == it->lfn_state->checksum) {
                    // Continuation
                } else {
                    // Invalid; drop
                    fat32_dir_iter_drop_lfn(it);
                    continue;
                }

                memcpy(it->lfn_state->name16 + (order - 1) * 13, entry->name1, sizeof(entry->name1));
                memcpy(it->lfn_state->name16 + (order - 1) * 13 + 5, entry->name2, sizeof(entry->name2));
                memcpy(it->lfn_state->name16 + (order - 1) * 13 + 11, entry->name3, sizeof(entry->name3));
                it->lfn_state->next_order--;

                continue;
            }

            struct fat32_directory_entry *entry = (struct fat32_directory_entry *)raw;
            if (entry->attr & FAT_ATTR_VOLUME_ID) {
                fat32_dir_iter_drop_lfn(it);
                continue;
            }

            item->entry = entry;

            uint8_t checksum = fat32_short_checksum(entry->name);
            if (checksum == it->lfn_state->checksum && it->lfn_state->total != 0 && it->lfn_state->next_order == 0) {
                if (fat32_utf16_to_utf8(it->lfn_state->name16, it->lfn_state->total * 13, item->name, sizeof(item->name)) < 0) {
                    // Fallback
                    fat32_short_name(entry, item->name);
                }
            } else {
                fat32_short_name(entry, item->name);
            }

            fat32_dir_iter_drop_lfn(it);

            return 1;
        }

        // Finished this cluster; go to the next one
        uint32_t next;
        if (fat32_next_cluster(it->fs, it->cluster, &next) < 0 || next == 0 || next >= FAT32_END_OF_CHAIN) {
            it->cluster = FAT32_END_OF_CHAIN;
            it->off     = 0;
            return 0;
        }
        it->cluster = next;
        it->off     = 0;
        it->loaded  = false;
    }

    return 0;
}

int fat32_load_func(fscache_node_t *node, const char *name, fscache_node_t *out) {
    vfs_fat32_cache_node_data_t *node_data = (vfs_fat32_cache_node_data_t *)node->internal_data;

    if (node_data->dir_entry->attr == FAT_ATTR_LONG_NAME) {
        kout(KERNEL_WARN, "fat32_load_func(): long entry support not implemented!\n");
        return -ENOSYS;
    }

    if (!(node_data->dir_entry->attr & FAT_ATTR_DIRECTORY)) {
        kout(KERNEL_PANIC, "fat32_load_func(): not a directory (so load_func shouldn't have been populated)!\n");
        return -ENOTDIR;
    }

    struct filesystem_fat32 *fs = node_data->fs;

    struct fat32_dir_iter it;
    int res = fat32_dir_iter_init(&it, fs, fat32_dir_start_cluster(node_data), 0);
    if (res < 0) {
        return res;
    }

    struct fat32_dir_item item;
    while ((res = fat32_dir_iter_next(&it, &item)) > 0) {
        if (strncmp(item.name, name, NAME_MAX + 1) != 0) {
            continue;
        }

        // Found
        struct fat32_directory_entry *entry   = item.entry;
        vfs_fat32_cache_node_data_t *out_data = (vfs_fat32_cache_node_data_t *)out->internal_data;
        out_data->dir_entry                   = kmalloc_heap(sizeof(struct fat32_directory_entry));
        if (!out_data->dir_entry) {
            kout(KERNEL_WARN, "fat32_load_func(): kmalloc_heap() failed after finding node (OOM?)!\n");
            res = -ENOMEM;
            goto cleanup;
        }
        *out_data->dir_entry = *entry;

        out_data->fs = fs;

        mode_t mode = 0777;
        if (entry->attr & FAT_ATTR_READ_ONLY) {
            mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
        }
        if (!fs->exec) {
            mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
        }

        if (entry->attr & FAT_ATTR_DIRECTORY) {
            mode |= S_IFDIR;
        } else {
            mode |= S_IFREG;
        }

        struct kstat stat = {
            .dev  = fs->partition->endpoint->dev,
            .gid  = fs->mount_gid,
            .uid  = fs->mount_uid,
            .mode = mode,
            .size = (off_t)entry->file_size,
            .rdev = (dev_t)0,
        };

        fscache_node_populate(out, 0, name, stat,
                              (entry->attr & FAT_ATTR_DIRECTORY) ? &fat32_directory_fsops : &fat32_file_fsops);

        res = 0;
        goto cleanup;
    }

    if (res == 0) {
        res = -ENOENT;
    }

cleanup:
    fat32_dir_iter_free(&it);
    return res;
}

int fat32_unload_func(fscache_node_t *node) {
    vfs_fat32_cache_node_data_t *node_data = (vfs_fat32_cache_node_data_t *)node->internal_data;

    if (node_data->dir_entry != &_fat32_placeholder_root) {
        kfree_heap(node_data->dir_entry);
    }

    return 0;
}

ssize_t fat32_directory_read_func(vfs_handle_t *handle, void *buf, size_t len) {
    vfs_fat32_cache_node_data_t *node_data        = (vfs_fat32_cache_node_data_t *)handle->backing_node->internal_data;
    vfs_fat32_handle_instance_data_t *handle_data = (vfs_fat32_handle_instance_data_t *)handle->instance_data;

    if (len % sizeof(struct plenjos_dirent)) {
        return -EINVAL;
    }

    if (node_data->dir_entry->attr == FAT_ATTR_LONG_NAME) {
        kout(KERNEL_WARN, "fat32_directory_read_func(): long entry support not implemented!\n");
        return -ENOSYS;
    }

    if (!(node_data->dir_entry->attr & FAT_ATTR_DIRECTORY)) {
        kout(KERNEL_PANIC,
             "fat32_directory_read_func(): not a directory (so read_func shouldn't have been populated)!\n");
        return -ENOTDIR;
    }

    if (handle_data->current_cluster == 0) {
        // First read on this handle
        handle_data->current_cluster = fat32_dir_start_cluster(node_data);
        handle_data->pos_in_cluster  = 0;
    }

    struct fat32_dir_iter it;
    int init_res = fat32_dir_iter_init(&it, node_data->fs, (uint32_t)handle_data->current_cluster,
                                       (uint32_t)handle_data->pos_in_cluster);
    if (init_res < 0) {
        return init_res;
    }

    ssize_t res = 0;
    struct fat32_dir_item item;

    while ((size_t)res < len) {
        int n = fat32_dir_iter_next(&it, &item);
        if (n < 0) {
            res = n;
            goto cleanup;
        }
        if (n == 0) {
            // End of directory
            handle_data->current_cluster = FAT32_END_OF_CHAIN;
            handle_data->pos_in_cluster  = 0;
            goto cleanup;
        }

        if (!strncmp(item.name, ".", 2) || !strncmp(item.name, "..", 3)) {
            continue;
        }

        struct plenjos_dirent *dirent = (struct plenjos_dirent *)((uint8_t *)buf + res);
        memset(dirent, 0, sizeof(struct plenjos_dirent));

        memcpy(dirent->d_name, item.name, sizeof(dirent->d_name) - 1); // the -1 keeps the NULL terminator
        dirent->type = (item.entry->attr & FAT_ATTR_DIRECTORY) ? DT_DIR : DT_REG;

        res += sizeof(struct plenjos_dirent);
    }

    // The buffer is full; remember where to pick up on the next call
    handle_data->current_cluster = it.cluster;
    handle_data->pos_in_cluster  = it.off;

cleanup:
    fat32_dir_iter_free(&it);
    if (res > 0) {
        handle_data->seek_pos += res;
    }
    return res;
}

int fat32_create_child_func(fscache_node_t *parent, const char *name, struct kstat stat, fscache_node_t *node) {
    return -ENOSYS;
}

ssize_t fat32_file_read_func(vfs_handle_t *handle, void *buf, size_t buf_len) {
    vfs_fat32_cache_node_data_t *node_data        = (vfs_fat32_cache_node_data_t *)handle->backing_node->internal_data;
    vfs_fat32_handle_instance_data_t *handle_data = (vfs_fat32_handle_instance_data_t *)handle->instance_data;

    if (node_data->dir_entry->attr == FAT_ATTR_LONG_NAME) {
        kout(KERNEL_WARN, "fat32_file_read_func(): long entry support not implemented!\n");
        return -ENOSYS;
    }

    if (node_data->dir_entry->attr & FAT_ATTR_DIRECTORY) {
        kout(KERNEL_PANIC, "fat32_file_read_func(): this is a directory (so this is the wrong read func!)!\n");
        return -ENOTDIR;
    }

    struct filesystem_fat32 *fs = node_data->fs;

    uint32_t cluster;
    if (handle_data->current_cluster == 0) {
        cluster                      = FAT32_CLUSTER_FROM_DIR_ENTRY(node_data->dir_entry);
        handle_data->current_cluster = cluster;
    } else {
        cluster = handle_data->current_cluster;
    }

    if (handle_data->seek_pos > node_data->dir_entry->file_size) {
        // Prevent underflow on subtraction later
        return 0;
    }

    size_t len = buf_len;
    if (len + handle_data->seek_pos > node_data->dir_entry->file_size) {
        len = node_data->dir_entry->file_size - handle_data->seek_pos;
    }

    uint32_t bytes_per_cluster = fs->boot_sector.generic.bytes_per_sector * fs->boot_sector.generic.sectors_per_cluster;

    uint8_t *cluster_buf = kmalloc_heap(bytes_per_cluster);
    if (!cluster_buf) {
        kout(KERNEL_WARN, "fat32_file_read_func(): couldn't allocate cluster_buf (kmalloc_heap() failed) (OOM?)\n");
        return -ENOMEM;
    }

    ssize_t res = 0;

    // printf("FAT32 Directory (start cluster %u):\n", cluster);

    while (cluster < 0x0FFFFFF8 && res < len) {
        int read_res = fat32_read_entry(fs, cluster, cluster_buf, bytes_per_cluster);

        if (read_res < 0) {
            kout(KERNEL_WARN, "fat32_file_read_func(): fat32_read_entry() failed with errno %d\n", read_res);
            if (res == 0) {
                res = -EIO;
            }
            goto cleanup;
        }

        if (bytes_per_cluster <= handle_data->pos_in_cluster) {
            kout(KERNEL_PANIC, "fat32_file_read_func(): handle_data->pos_in_cluster is >= bytes_per_cluster!\n");
            if (res == 0) {
                res = -EIO;
            }
            goto cleanup;
        }

        if (res == 0) {
            // First cluster
            res = (size_t)bytes_per_cluster - handle_data->pos_in_cluster;
            if (res > len) {
                res = len;
                memcpy(buf, cluster_buf + handle_data->pos_in_cluster, res);
                handle_data->pos_in_cluster += res;
                goto cleanup;
            }
            memcpy(buf, cluster_buf + handle_data->pos_in_cluster, res);
            handle_data->pos_in_cluster = 0;
        } else {
            size_t to_copy = bytes_per_cluster;
            if (res + to_copy > len) {
                to_copy = len - res;
                memcpy((uint8_t *)buf + res, cluster_buf, to_copy);
                handle_data->pos_in_cluster += to_copy;
                res                          = len;
                goto cleanup;
            }
            memcpy((uint8_t *)buf + res, cluster_buf, to_copy);
            res += to_copy;
        }

        uint32_t next;
        int next_res = fat32_next_cluster(fs, cluster, &next);
        if (next_res < 0) {
            kout(KERNEL_SEVERE_EXTERNAL_FAULT,
                 "fat32_file_read_func(): fat32_next_cluster failed (errno %d); marking as EOF to prevent more broken "
                 "reads.\n",
                 next_res);
            handle_data->current_cluster = 0x0FFFFFF8;
            break;
        }
        if (next == 0 || next >= 0x0FFFFFF8) {
            handle_data->current_cluster = 0x0FFFFFF8;
            break;
        }
        cluster                      = next;
        handle_data->current_cluster = next;
    }

cleanup:
    kfree_heap(cluster_buf);
    if (res > 0) {
        handle_data->seek_pos += res;
    }
    return res;
}