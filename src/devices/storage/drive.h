#pragma once

#include "kernel.h"

#include <stddef.h>
#include <stdint.h>

typedef struct DRIVE DRIVE_t;

typedef ssize_t (*drive_read_sectors_func_t)(DRIVE_t *drive, uint64_t lba, size_t sectors, void *buffer);
typedef ssize_t (*drive_write_sectors_func_t)(DRIVE_t *drive, uint64_t lba, size_t sectors, const void *buffer);

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

    void *internal_data;
};

