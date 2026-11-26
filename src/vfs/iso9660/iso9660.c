#include "iso9660.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "plenjos/errno.h"
#include "vfs/fscache.h"

static int iso9660_load_func(fscache_node_t *node, const char *name, fscache_node_t *out);
static int iso9660_unload_func(fscache_node_t *node);
static ssize_t iso9660_directory_read_func(vfs_handle_t *handle, void *buf, size_t len);
static int iso9660_create_child_func(fscache_node_t *parent, const char *name, struct kstat stat, fscache_node_t *node);
static ssize_t iso9660_file_read_func(vfs_handle_t *handle, void *buf, size_t len);

static const vfs_ops_block_t iso9660_file_fsops = {
    .read        = iso9660_file_read_func,
    .load_node   = iso9660_load_func,
    .unload_node = iso9660_unload_func,
};
static const vfs_ops_block_t iso9660_directory_fsops = {
    .read         = iso9660_directory_read_func,
    .load_node    = iso9660_load_func,
    .create_child = iso9660_create_child_func,
    .unload_node  = iso9660_unload_func,
};

/**
 * For caching purposes, we will ignore the path table, as it's easier to only cache directory records (all directories
 * are in both path table and directory records).
 */

static size_t iso9660_normalize_name(char *out, size_t out_size, const uint8_t *in, size_t in_len) {
    size_t o = 0;
    size_t i = 0;

    /* Handle special entries */
    if (in_len == 1 && in[0] == 0) {
        if (out_size > 1) {
            out[0] = '.';
            out[1] = '\0';
        }
        return 1;
    }
    if (in_len == 1 && in[0] == 1) {
        if (out_size > 2) {
            out[0] = '.';
            out[1] = '.';
            out[2] = '\0';
        }
        return 2;
    }

    while (i < in_len && o + 1 < out_size) {
        uint8_t c = in[i++];

        /* Stop at version separator */
        if (c == ';') break;

        /* Convert to lowercase */
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';

        out[o++] = c;
    }

    out[o] = '\0';

    // Remove trailing dot if present (for files without extensions)
    if (o > 0 && out[o - 1] == '.') {
        out[--o] = '\0';
    }

    return o;
}

static ssize_t iso9660_file_read_func(vfs_handle_t *handle, void *buf, size_t len) {
    if (!handle || !buf) return -EINVAL;

    if (len == 0) return 0;

    if (!handle->backing_node) return -EIO;

    vfs_iso9660_cache_node_data_t *node_data = (vfs_iso9660_cache_node_data_t *)handle->backing_node->internal_data;

    if (!node_data || !node_data->fs || !node_data->dir_record) return -EIO;

    struct filesystem_iso9660 *fs                     = node_data->fs;
    struct iso9660_directory_record *record           = node_data->dir_record;
    vfs_iso9660_handle_instance_data_t *instance_data = (vfs_iso9660_handle_instance_data_t *)handle->instance_data;

    uint64_t file_size = record->data_length_le;
    uint64_t pos       = instance_data->seek_pos;

    if (pos >= file_size) return 0; /* EOF */

    /* Clamp read length to file size */
    if (len > file_size - pos) len = file_size - pos;

    uint64_t block_size = fs->logical_block_size;
    uint64_t start_lba  = record->extent_location_lba_le;
    uint64_t cur_block  = pos / block_size;
    uint64_t block_off  = pos % block_size;

    uint8_t *block_buf = kmalloc_heap(block_size);
    if (!block_buf) return -ENOMEM;

    size_t remaining = len;
    size_t copied    = 0;

    while (remaining > 0) {
        uint64_t lba = fs->partition_start_lba + start_lba + cur_block;

        ssize_t res = fs->drive->read_sectors(fs->drive, lba, 1, block_buf);
        if (res < 0) {
            kfree_heap(block_buf);
            return -EIO;
        }

        size_t avail   = block_size - block_off;
        size_t to_copy = remaining < avail ? remaining : avail;

        memcpy((uint8_t *)buf + copied, block_buf + block_off, to_copy);

        copied    += to_copy;
        remaining -= to_copy;
        block_off  = 0;
        cur_block++;
    }

    instance_data->seek_pos += copied;
    kfree_heap(block_buf);
    return (ssize_t)copied;
}

static ssize_t iso9660_directory_read_func(vfs_handle_t *handle, void *buf, size_t len) {
    if (handle == NULL || buf == NULL) {
        return -EINVAL;
    }

    if (len % sizeof(struct plenjos_dirent)) {
        return -EINVAL;
    }

    if (handle->backing_node == NULL) {
        return -EIO;
    }

    // Find the parent node's ISO9660 data
    vfs_iso9660_cache_node_data_t *parent_data = (vfs_iso9660_cache_node_data_t *)handle->backing_node->internal_data;
    if (parent_data->fs == NULL || parent_data->dir_record == NULL) {
        return -EIO;
    }

    vfs_iso9660_handle_instance_data_t *instance_data = (vfs_iso9660_handle_instance_data_t *)handle->instance_data;

    struct filesystem_iso9660 *fs = parent_data->fs;

    uint64_t dir_extent_lba = parent_data->dir_record->extent_location_lba_le + instance_data->current_extent_location
                              + fs->partition_start_lba;
    uint64_t dir_size       = parent_data->dir_record->data_length_le - instance_data->seek_pos;
    uint64_t logical_block_size = fs->logical_block_size;

    uint64_t seek_offset_in_block = instance_data->seek_pos % logical_block_size;
    if (seek_offset_in_block != instance_data->extent_pos) {
        // Node data is inconsistent
        printf("iso9660_directory_read_func: inconsistent seek position (%lu) and extent position (%lu)\n",
               instance_data->seek_pos, instance_data->extent_pos);
        return -EIO;
    }

    uint8_t *buffer = kmalloc_heap(logical_block_size);
    if (buffer == NULL) {
        printf("OOM Error: iso9660_directory_read_func: could not allocate memory for directory read buffer\n");
        return -ENOMEM;
    }

    ssize_t total_bytes_read = 0;
    struct iso9660_directory_record *entry;
    uint64_t offs;
    ssize_t res;
    size_t name_len;

    uint64_t block_offset = instance_data->extent_pos;

    while (dir_size > 0 && len >= sizeof(struct plenjos_dirent)) {
        // Read current logical block
        res = fs->drive->read_sectors(fs->drive, dir_extent_lba, 1, buffer);
        if (res < 0) {
            printf("iso9660_directory_read_func: error reading directory sector\n");
            kfree_heap(buffer);
            return -EIO;
        }

        offs = 0;
        while (offs < logical_block_size && len >= sizeof(struct plenjos_dirent) && dir_size > 0) {
            entry = (struct iso9660_directory_record *)(buffer + offs);

            if (entry->length == 0) {
                // No more entries in this block
                break;
            }

            if (entry->length > logical_block_size - offs) {
                printf("iso9660_directory_read_func: invalid directory record length\n");
                kfree_heap(buffer);
                return -EIO;
            }

            if (block_offset >= entry->length) {
                block_offset -= entry->length;
                offs         += entry->length;
                continue;
            } else if (block_offset != 0) {
                return -EIO; // illegal state
            }

            // Fill in dirent
            name_len = entry->file_name_length;
            if (name_len > NAME_MAX) name_len = NAME_MAX;

            struct plenjos_dirent *dent = (struct plenjos_dirent *)((uint8_t *)buf + total_bytes_read);
            memset(dent, 0, sizeof(struct plenjos_dirent));
            iso9660_normalize_name(dent->d_name, NAME_MAX + 1, (uint8_t *)entry->file_identifier, name_len);

            if (dent->d_name[0] == '\0') {
                // Should not happen, but just in case
                strncpy(dent->d_name, "unknown", NAME_MAX);
                dent->d_name[NAME_MAX] = '\0';
            }

            if (dent->d_name[0] == '.' && dent->d_name[1] == '\0') {
                // Special case for "."
                memset(dent->d_name, 0, NAME_MAX);
            } else if (dent->d_name[0] == '.' && dent->d_name[1] == '.' && dent->d_name[2] == '\0') {
                // Special case for ".."
                memset(dent->d_name, 0, NAME_MAX);
            } else {
                dent->type = (entry->file_flags & 0x02) ? DT_DIR : DT_REG;

                total_bytes_read += sizeof(struct plenjos_dirent);
                len              -= sizeof(struct plenjos_dirent);
            }

            // Advance offsets
            offs                      += entry->length;
            instance_data->seek_pos   += entry->length;
            instance_data->extent_pos += entry->length;
            dir_size                  -= entry->length;
        }

        if (offs < logical_block_size) {
            // We still have more entries in this block; stop here to stay on this block for the next read
            break;
        }
        // Move to next block
        dir_extent_lba++;
        instance_data->current_extent_location++;
        block_offset = instance_data->extent_pos = 0;
    }

    kfree_heap(buffer);
    return (ssize_t)total_bytes_read;
}

static int iso9660_unload_func(fscache_node_t *node) {
    if (!node) {
        return -EINVAL;
    }

    vfs_iso9660_cache_node_data_t *node_data = (vfs_iso9660_cache_node_data_t *)node->internal_data;
    if (node_data->dir_record) {
        kfree_heap(node_data->dir_record);
    }

    return 0;
}

static int iso9660_load_func(fscache_node_t *node, const char *name, fscache_node_t *out) {
    if (!node || !name || !out) {
        return -EINVAL;
    }

    vfs_iso9660_cache_node_data_t *parent_data = (vfs_iso9660_cache_node_data_t *)node->internal_data;

    struct filesystem_iso9660 *fs = parent_data->fs;
    if (!fs) {
        return -EIO;
    }

    // Make sure parent is a directory
    if (!(parent_data->dir_record->file_flags & 0x02)) {
        printf("iso9660: parent node is not a directory\n");
        return -ENOTDIR;
    }

    uint64_t dir_extent_lba     = parent_data->dir_record->extent_location_lba_le;
    uint64_t dir_size           = parent_data->dir_record->data_length_le;
    uint64_t logical_block_size = fs->logical_block_size;

    uint8_t *dir_buffer = kmalloc_heap(logical_block_size);
    if (!dir_buffer) {
        printf("OOM Error: iso9660: could not allocate memory for directory read buffer\n");
        return -ENOMEM;
    }

    // This is small enough to fit on the stack
    char buf[NAME_MAX + 1];

    while (dir_size > 0) {
        ssize_t res = fs->drive->read_sectors(fs->drive, dir_extent_lba, 1, dir_buffer);
        if (res < 0) {
            printf("iso9660: error reading directory sector\n");
            kfree_heap(dir_buffer);
            return -EIO;
        }

        uint64_t offs = 0;
        while (offs < logical_block_size) {
            struct iso9660_directory_record *dir_record = (struct iso9660_directory_record *)(dir_buffer + offs);
            if (dir_record->length == 0) {
                // No more records in this block
                break;
            }
            if (dir_record->length < 1 || dir_record->length > logical_block_size) {
                printf("iso9660_load_func: invalid directory record length\n");
                kfree_heap(dir_buffer);
                return -EIO;
            }

            iso9660_normalize_name((char *)buf, NAME_MAX + 1, (uint8_t *)dir_record->file_identifier,
                                   dir_record->file_name_length);

            if (strlen(buf) == strlen(name) && strncmp(buf, name, strlen(name)) == 0) {
                // Found it
                vfs_iso9660_cache_node_data_t *child_data = (vfs_iso9660_cache_node_data_t *)out->internal_data;

                child_data->fs         = fs;
                child_data->dir_record = kmalloc_heap(dir_record->length);
                if (!child_data->dir_record) {
                    printf("OOM Error: iso9660: could not allocate memory for directory record copy\n");
                    kfree_heap(dir_buffer);
                    return -ENOMEM;
                }
                memcpy(child_data->dir_record, dir_record, dir_record->length);

                child_data->unused[0] = 0;
                child_data->unused[1] = 0;

                struct kstat stat = { 0 };
                stat.dev          = child_data->fs->drive->endpoint->dev;
                stat.mode         = S_DFLT | ((dir_record->file_flags & 0x02) ? S_IFDIR : S_IFREG);
                stat.size         = dir_record->data_length_le;
                fscache_node_populate(out, 0, name, stat,
                                      (dir_record->file_flags & 0x02) ? &iso9660_directory_fsops : &iso9660_file_fsops);

                kfree_heap(dir_buffer);
                return 0;
            }
            offs += dir_record->length;
        }
        dir_extent_lba++;
        if (dir_size > logical_block_size) {
            dir_size -= logical_block_size;
        } else {
            dir_size = 0;
        }
    }

    kfree_heap(dir_buffer);
    return -ENOENT;
}

static int iso9660_create_child_func(fscache_node_t *parent, const char *name, struct kstat stat, fscache_node_t *node) {
    // To be implemented
    return -ENOSYS;
}

int iso9660_setup(struct filesystem_iso9660 *fs, DRIVE_t *drive, partition_t *partition, uint32_t partition_start_lba,
                  fscache_node_t *mount_node) {
    if (!fs || !drive || !(fs->read_status & 0x01)) {
        return -EINVAL;
    }

    fs->drive               = drive;
    fs->partition           = partition;
    fs->partition_start_lba = partition_start_lba;

    struct iso9660_primary_volume_descriptor *pvd = &fs->pvd;
    fs->logical_block_size                        = pvd->logical_block_size_le;

    fs->path_table_size            = pvd->path_table_size_le;
    fs->type_l_path_table_location = pvd->type_l_path_table_location;
    fs->type_m_path_table_location = pvd->type_m_path_table_location;
    fs->root_directory_extent_location
        = ((struct iso9660_directory_record *)pvd->root_directory_record)->extent_location_lba_le;
    fs->root_directory_size = ((struct iso9660_directory_record *)pvd->root_directory_record)->data_length_le;

    mount_node->fsops                        = &iso9660_directory_fsops;
    // TODO: decide if we want to support xattr for ownership
    vfs_iso9660_cache_node_data_t *node_data = (vfs_iso9660_cache_node_data_t *)mount_node->internal_data;
    node_data->fs                            = fs;
    node_data->dir_record                    = kmalloc_heap(sizeof(struct iso9660_directory_record));
    if (!node_data->dir_record) {
        printf("OOM Error: iso9660_setup: could not allocate memory for root directory record\n");
        return -ENOMEM;
    }
    memcpy(node_data->dir_record, pvd->root_directory_record, sizeof(struct iso9660_directory_record));

    kout(KERNEL_INFO,
         "ISO9660 filesystem mounted: logical block size %d, root directory extent LBA %d, size %d bytes\n",
         fs->logical_block_size, fs->root_directory_extent_location, fs->root_directory_size);

    return 0;
}