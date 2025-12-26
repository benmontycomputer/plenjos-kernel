#include "fscommon.h"

#include "lib/stdio.h"

int read_first_sector(DRIVE_t *drive, uint32_t partition_start_lba, uint8_t *buffer) {
    if (!buffer) {
        return -1;
    }

    // Read boot sector
    ssize_t res = drive->read_sectors(drive, partition_start_lba, 1, buffer);
    if (res < 0) {
        printf("Error: Could not read FAT boot sector\n");
        return -1;
    }

    return 0;
}