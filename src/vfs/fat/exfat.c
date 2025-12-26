#include "exfat.h"

#include "lib/stdio.h"

#include <stddef.h>
#include <stdint.h>

int exfat_setup(struct filesystem_exfat *fs, DRIVE_t *drive, uint32_t partition_start_lba) {
    if (!fs || !drive) {
        return -1;
    }

    if (!(fs->read_status & 0x01)) {
        printf("exFAT: boot sector must be read before setup\n");
        return -1;
    }

    fs->drive               = drive;
    fs->partition_start_lba = partition_start_lba;
    fs->fat_size_sectors    = fs->boot_sector.FatLength;
    fs->cluster_heap_start_lba = fs->boot_sector.ClusterHeapOffset;
    fs->total_sectors         = (uint32_t)fs->boot_sector.VolumeLength;
    fs->total_clusters        = fs->boot_sector.ClusterCount;

    printf("exFAT filesystem setup successful. Total clusters: %u, Size: %u bytes\n", fs->total_clusters,
           fs->total_sectors * (1 << fs->boot_sector.BytesPerSectorShift));
    
    return 0;
}