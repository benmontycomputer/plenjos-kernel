/**
 * @file ide.h
 * @brief IDE Support Header
 *
 * Header file for IDE storage device support. Requires ATA support.
 *
 * @pre ATA support
 */

#pragma once

#include "devices/manager.h"

#ifdef __KERNEL_SUPPORT_DEV_STORAGE_IDE

# ifndef __KERNEL_SUPPORT_DEV_STORAGE_ATA
#  error "IDE support requires storage ATA support to be enabled"
# else
#  include "ata/ata.h"
# endif

# include "drive.h"
# include "lib/lock.h"

# include <stdatomic.h>
# include <stdint.h>

/**
 * Fixed I/O ports for IDE controllers in compatibility mode
 */
# define IDE_PRIMARY_CMD_BASE    0x1F0
# define IDE_PRIMARY_CTRL_BASE   0x3F6
# define IDE_SECONDARY_CMD_BASE  0x170
# define IDE_SECONDARY_CTRL_BASE 0x376

/**
 * For use in ide_device_flags_t; not related to any ATA standard
 */
# define IDE_DEVICE_FLAG_PRESENT     0x0001
# define IDE_DEVICE_FLAG_SLAVE       0x0002
# define IDE_DEVICE_FLAG_LBA48       0x0004
# define IDE_DEVICE_FLAG_ATAPI       0x0008
# define IDE_DEVICE_FLAG_DMA_SUPPORT 0x0010
# define IDE_DEVICE_FLAG_DMA_ENABLED 0x0020
# define IDE_DEVICE_FLAG_REMOVABLE   0x0040

typedef uint32_t ide_device_flags_t;

struct ide_channel {
    uint16_t cmd_base;
    uint16_t ctrl_base;
    uint8_t irq_no;

    // How many IRQs have been fired for this channel and are being processed
    atomic_int irq_cnt;

    // Last read status register value (from IRQ handler)
    atomic_uchar last_status;

    mutex lock;
    int selected; // -1 = none, 0 = master, 1 = slave
};

struct ide_device {
    struct ide_channel *channel;
    int drive_no; // 0=master, 1=slave

    enum ata_device_type type;

    // Bytes, not words
    uint32_t logical_sector_size;
    uint32_t physical_sector_size;

    ide_device_flags_t flags;
    uint64_t numsectors;

    union {
        struct ata_info info;
        struct atapi_info atapi_info;
    };

    union {
        uint16_t identify_raw[256];
        struct ata_identify identify;
        struct atapi_identify atapi_identify;
    };

    struct DRIVE drive;
};

void ide_init();

int ide_lock_bus(struct ide_device *dev);
int ide_unlock_bus(struct ide_device *dev);

// This also serves to lock the bus (as long as 0 is returned)
int ide_select_bus(struct ide_device *dev);
int ide_wait_for_irq(struct ide_device *dev);

#endif // __KERNEL_SUPPORT_DEV_STORAGE_IDE