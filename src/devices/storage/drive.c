#include "drive.h"

#include "devices/sysdrivers.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "mbr.h"
#include "memory/kmalloc.h"
#include "plenjos/errno.h"
#include "plenjos/sysmacros.h"
#include "vfs/fscache.h"
#include "vfs/kernelfs.h"
#include "vfs/vfs.h"

int next_free_drive = 1; // Reserve drive 0 for boot

#define DRIVE_NUM_SHIFT 8
#define MAX_DRIVES      0x1000

ssize_t drive_endpoint_read(vfs_handle_t *handle, void *buf, size_t len);
ssize_t drive_endpoint_write(vfs_handle_t *handle, const void *buf, size_t len);
off_t drive_endpoint_seek(vfs_handle_t *handle, off_t offset, vfs_seek_whence_t whence);

// Driver for both whole drives and partitions
endpoint_driver_t _drive_endpoint_driver = {
    .flags = ENDPOINT_DRIVER_FLAG_BLOCK,
    .major = ENDPOINT_DRIVE,
    .read  = drive_endpoint_read,
    .write = drive_endpoint_write,
    .seek  = drive_endpoint_seek,
};

void populate_and_register_drive(DRIVE_t *drive, uint64_t numsectors, uint32_t logical_sector_size,
                                 uint32_t physical_sector_size, char *model, char *interface,
                                 drive_read_sectors_func_t read_sectors, drive_write_sectors_func_t write_sectors,
                                 uint32_t irq, void *internal_data) {
    drive->numsectors           = numsectors;
    drive->logical_sector_size  = logical_sector_size;
    drive->physical_sector_size = physical_sector_size;
    strncpy(drive->model, model, 40);
    drive->model[40] = '\0';
    strncpy(drive->interface, interface, 15);
    drive->interface[15] = '\0';
    drive->read_sectors  = read_sectors;
    drive->write_sectors = write_sectors;
    drive->irq           = irq;
    drive->internal_data = internal_data;

    drive->first_partition = NULL;

    struct MBR mbr = { 0 };
    drive_read_mbr(drive, &mbr);

    int drive_no = 0;

    uint32_t mbr_disk_signature = *(uint32_t *)mbr.disk_signature;

    if (mbr_disk_signature && mbr_disk_signature == executable_file_request.response->executable_file->mbr_disk_id) {
        kout(KERNEL_INFO, "Found boot drive.\n");
    } else {
        // NOT the minor number of the endpoint; have to bitshift to get that
        drive_no = next_free_drive;
        next_free_drive++;
        if (drive_no >= MAX_DRIVES) {
            kout(KERNEL_SEVERE_FAULT, "populate_and_register_drive(): no more free drives! (should never happen)\n");
        }
    }

    char name[NAME_MAX + 1];
    strncpy(name, "drive", 5);

    char *name_end     = name + strlen("drive");
    uint32_t drive_hex = (uint32_t)drive_no;
    int started        = 0;

    for (int shift = 28; shift >= 0; shift -= 4) {
        uint32_t nibble = (drive_hex >> shift) & 0xF;

        if (!started) {
            if (nibble == 0 && shift != 0) {
                continue;
            }

            started = 1;
        }

        *name_end++ = "0123456789abcdef"[nibble];
    }

    *name_end = '\0';

    // TODO: sysfs: pass actual physical device here
    drive->endpoint = register_device_endpoint(name, &_drive_endpoint_driver, drive_no << DRIVE_NUM_SHIFT, NULL, drive);

    if (mbr_disk_signature && mbr_disk_signature == executable_file_request.response->executable_file->mbr_disk_id) {
        // TODO: better cmdline parsing
        int partition_no = executable_cmdline_request.response->cmdline[0] - '0';
        if (partition_no < 1 || partition_no > 4) {
            kout(KERNEL_WARN, "WARN: Bad partition no %d. Can't find root.\n", partition_no);
        } else {
            int res = mbr_mount(drive, partition_no, (uid_t)0, "/");
            if (res) {
                kout(KERNEL_WARN, "Failed to mount root dir (partition %d): res %d\n", partition_no, res);
            }
            mount_devfs();
        }
    }
}

ssize_t drive_endpoint_read(vfs_handle_t *handle, void *buf, size_t len) {
    if (!handle || !buf) {
        return -EINVAL;
    }

    if (!handle->backing_node) {
        return -EIO;
    }

    dev_t rdev = handle->backing_node->stat.rdev;
    if (major(rdev) != ENDPOINT_DRIVE) {
        kout(KERNEL_SEVERE_FAULT, "drive_endpoint_read(): not a drive!\n");
        return -EINVAL;
    }

    void *func_args = kernelfs_get_func_args(handle);
    if (!func_args) {
        return -EIO;
    }

    kernelfs_handle_instance_data_t *handle_data = (kernelfs_handle_instance_data_t *)handle->instance_data;
    uint64_t pos_bytes                           = handle_data->pos;
    uint64_t endpoint_offs_lba                   = 0;

    DRIVE_t *drive = NULL;

    if (minor(rdev) == 0) {
        drive = (DRIVE_t *)func_args;
    } else {
        // TODO: this needs to change once GPT implemented
        partition_t *part = (partition_t *)func_args;
        drive             = part->drive;
        endpoint_offs_lba = (uint64_t)part->lba_act;
    }

    if (!drive || !drive->read_sectors || drive->logical_sector_size == 0) {
        return -EIO;
    }

    uint64_t pos_sect    = pos_bytes / drive->logical_sector_size;
    uint64_t pos_in_sect = pos_bytes % drive->logical_sector_size;

    uint64_t available_bytes = drive->numsectors * (uint64_t)drive->logical_sector_size;
    if (pos_bytes >= available_bytes) {
        return 0;
    }

    size_t bytes_to_read     = len;
    uint64_t bytes_remaining = available_bytes - pos_bytes;
    if (bytes_to_read > bytes_remaining) {
        bytes_to_read = (size_t)bytes_remaining;
    }

    if (bytes_to_read == 0) {
        return 0;
    }

    uint8_t *sector_buf = kmalloc_heap(drive->logical_sector_size);
    if (!sector_buf) {
        return -ENOMEM;
    }

    size_t bytes_copied          = 0;
    uint64_t first_lba           = endpoint_offs_lba + pos_sect;
    size_t first_partial_len     = min((size_t)drive->logical_sector_size - (size_t)pos_in_sect, bytes_to_read);
    size_t remaining_after_first = bytes_to_read - first_partial_len;
    size_t middle_sector_count   = remaining_after_first / drive->logical_sector_size;
    size_t last_partial_len      = remaining_after_first % drive->logical_sector_size;
    uint64_t middle_lba          = first_lba + (pos_in_sect ? 1 : 0);
    uint64_t last_lba            = middle_lba + middle_sector_count;

    if (pos_in_sect) {
        ssize_t read_res = drive->read_sectors(drive, first_lba, 1, sector_buf);
        if (read_res < 0) {
            kfree_heap(sector_buf);
            return read_res;
        }

        if ((size_t)read_res < drive->logical_sector_size) {
            kfree_heap(sector_buf);
            return -EIO;
        }

        memcpy((uint8_t *)buf, sector_buf + pos_in_sect, first_partial_len);
        bytes_copied += first_partial_len;
    }

    if (middle_sector_count > 0) {
        uint8_t *middle_dest = (uint8_t *)buf + bytes_copied;
        ssize_t read_res     = drive->read_sectors(drive, middle_lba, middle_sector_count, middle_dest);
        if (read_res < 0) {
            kfree_heap(sector_buf);
            return (bytes_copied > 0) ? (ssize_t)bytes_copied : read_res;
        }

        size_t expected_bytes = middle_sector_count * drive->logical_sector_size;
        if ((size_t)read_res < expected_bytes) {
            kfree_heap(sector_buf);
            return (bytes_copied > 0) ? (ssize_t)bytes_copied : -EIO;
        }

        bytes_copied += expected_bytes;
    }

    if (last_partial_len > 0) {
        ssize_t read_res = drive->read_sectors(drive, last_lba, 1, sector_buf);
        if (read_res < 0) {
            kfree_heap(sector_buf);
            return (bytes_copied > 0) ? (ssize_t)bytes_copied : read_res;
        }

        if ((size_t)read_res < drive->logical_sector_size) {
            kfree_heap(sector_buf);
            return (bytes_copied > 0) ? (ssize_t)bytes_copied : -EIO;
        }

        memcpy((uint8_t *)buf + bytes_copied, sector_buf, last_partial_len);
        bytes_copied += last_partial_len;
    }

    kfree_heap(sector_buf);
    handle_data->pos += bytes_copied;
    return (ssize_t)bytes_copied;
}

ssize_t drive_endpoint_write(vfs_handle_t *handle, const void *buf, size_t len) {
    if (!handle || !buf) {
        return -EINVAL;
    }

    if (!handle->backing_node) {
        return -EIO;
    }

    dev_t rdev = handle->backing_node->stat.rdev;
    if (major(rdev) != ENDPOINT_DRIVE) {
        kout(KERNEL_SEVERE_FAULT, "drive_endpoint_write(): not a drive!\n");
        return -EINVAL;
    }

    void *func_args = kernelfs_get_func_args(handle);
    if (!func_args) {
        return -EIO;
    }

    kernelfs_handle_instance_data_t *handle_data = (kernelfs_handle_instance_data_t *)handle->instance_data;
    uint64_t pos_bytes                           = handle_data->pos;
    uint64_t endpoint_offs_lba                   = 0;

    DRIVE_t *drive = NULL;

    if (minor(rdev) == 0) {
        drive = (DRIVE_t *)func_args;
    } else {
        partition_t *part = (partition_t *)func_args;
        drive             = part->drive;
        endpoint_offs_lba = (uint64_t)part->lba_act;
    }

    if (!drive || !drive->read_sectors || !drive->write_sectors || drive->logical_sector_size == 0) {
        return -EIO;
    }

    uint64_t pos_sect    = pos_bytes / drive->logical_sector_size;
    uint64_t pos_in_sect = pos_bytes % drive->logical_sector_size;

    uint64_t available_bytes = drive->numsectors * (uint64_t)drive->logical_sector_size;
    if (pos_bytes >= available_bytes) {
        return 0;
    }

    size_t bytes_to_write    = len;
    uint64_t bytes_remaining = available_bytes - pos_bytes;
    if (bytes_to_write > bytes_remaining) {
        bytes_to_write = (size_t)bytes_remaining;
    }

    if (bytes_to_write == 0) {
        return 0;
    }

    uint8_t *sector_buf = kmalloc_heap(drive->logical_sector_size);
    if (!sector_buf) {
        return -ENOMEM;
    }

    size_t bytes_written         = 0;
    uint64_t first_lba           = endpoint_offs_lba + pos_sect;
    size_t first_partial_len     = min((size_t)drive->logical_sector_size - (size_t)pos_in_sect, bytes_to_write);
    size_t remaining_after_first = bytes_to_write - first_partial_len;
    size_t middle_sector_count   = remaining_after_first / drive->logical_sector_size;
    size_t last_partial_len      = remaining_after_first % drive->logical_sector_size;
    uint64_t middle_lba          = first_lba + (pos_in_sect ? 1 : 0);
    uint64_t last_lba            = middle_lba + middle_sector_count;

    if (pos_in_sect) {
        ssize_t read_res = drive->read_sectors(drive, first_lba, 1, sector_buf);
        if (read_res < 0) {
            kfree_heap(sector_buf);
            return read_res;
        }

        if ((size_t)read_res < drive->logical_sector_size) {
            kfree_heap(sector_buf);
            return -EIO;
        }

        memcpy(sector_buf + pos_in_sect, buf, first_partial_len);

        ssize_t write_res = drive->write_sectors(drive, first_lba, 1, sector_buf);
        if (write_res < 0) {
            kfree_heap(sector_buf);
            return write_res;
        }

        if ((size_t)write_res < drive->logical_sector_size) {
            kfree_heap(sector_buf);
            return -EIO;
        }

        bytes_written += first_partial_len;
    }

    if (middle_sector_count > 0) {
        const uint8_t *middle_src = (const uint8_t *)buf + bytes_written;
        size_t middle_bytes       = middle_sector_count * drive->logical_sector_size;

        ssize_t write_res = drive->write_sectors(drive, middle_lba, middle_sector_count, middle_src);
        if (write_res < 0) {
            kfree_heap(sector_buf);
            return (bytes_written > 0) ? (ssize_t)bytes_written : write_res;
        }

        if ((size_t)write_res < middle_bytes) {
            kfree_heap(sector_buf);
            return (bytes_written > 0) ? (ssize_t)bytes_written : -EIO;
        }

        bytes_written += middle_bytes;
    }

    if (last_partial_len > 0) {
        ssize_t read_res = drive->read_sectors(drive, last_lba, 1, sector_buf);
        if (read_res < 0) {
            kfree_heap(sector_buf);
            return (bytes_written > 0) ? (ssize_t)bytes_written : read_res;
        }

        if ((size_t)read_res < drive->logical_sector_size) {
            kfree_heap(sector_buf);
            return (bytes_written > 0) ? (ssize_t)bytes_written : -EIO;
        }

        memcpy(sector_buf, (const uint8_t *)buf + bytes_written, last_partial_len);

        ssize_t write_res = drive->write_sectors(drive, last_lba, 1, sector_buf);
        if (write_res < 0) {
            kfree_heap(sector_buf);
            return (bytes_written > 0) ? (ssize_t)bytes_written : write_res;
        }

        if ((size_t)write_res < drive->logical_sector_size) {
            kfree_heap(sector_buf);
            return (bytes_written > 0) ? (ssize_t)bytes_written : -EIO;
        }

        bytes_written += last_partial_len;
    }

    kfree_heap(sector_buf);
    handle_data->pos += bytes_written;
    return (ssize_t)bytes_written;
}

off_t drive_endpoint_seek(vfs_handle_t *handle, off_t offset, vfs_seek_whence_t whence) {
    if (!handle) {
        return -EINVAL;
    }

    if (!handle->backing_node) {
        return -EIO;
    }

    dev_t rdev = handle->backing_node->stat.rdev;
    if (major(rdev) != ENDPOINT_DRIVE) {
        kout(KERNEL_SEVERE_FAULT, "drive_endpoint_seek(): not a drive!\n");
        return -EINVAL;
    }

    void *func_args = kernelfs_get_func_args(handle);
    if (!func_args) {
        return -EIO;
    }

    kernelfs_handle_instance_data_t *handle_data = (kernelfs_handle_instance_data_t *)handle->instance_data;

    DRIVE_t *drive = NULL;
    if (minor(rdev) == 0) {
        drive = (DRIVE_t *)func_args;
    } else {
        partition_t *part = (partition_t *)func_args;
        drive             = part->drive;
    }

    if (!drive || drive->logical_sector_size == 0) {
        return -EIO;
    }

    int64_t current_pos = (int64_t)handle_data->pos;
    int64_t end_pos     = (int64_t)(drive->numsectors * (uint64_t)drive->logical_sector_size);
    int64_t new_pos     = 0;

    switch (whence) {
    case VFS_SEEK_SET:
        new_pos = (int64_t)offset;
        break;
    case VFS_SEEK_CUR:
        new_pos = current_pos + (int64_t)offset;
        break;
    case VFS_SEEK_END:
        new_pos = end_pos + (int64_t)offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_pos < 0) {
        return -EINVAL;
    }

    handle_data->pos = (uint64_t)new_pos;
    return (off_t)handle_data->pos;
}