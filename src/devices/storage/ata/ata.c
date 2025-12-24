#include "ata.h"

#include "devices/io/ports.h"
#include "devices/storage/ide.h"

int ata_wait_bsy_read_status(uint16_t io) {
    for (int i = 0; i < 100000; i++) {
        uint8_t st = inb(io + ATA_REG_STATUS);
        if (!(st & ATA_SR_BSY)) return (int)st;
    }
    return -1;
}

int ata_wait_drq(uint16_t io) {
    for (int i = 0; i < 100000; i++) {
        uint8_t st = inb(io + ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return -1;
        if (st & ATA_SR_DRQ) return 0;
    }
    return -1;
}

int ata_parse_identify(struct ide_device *dev) {
    /**
     * Process IDENTIFY data here as needed. Here are some useful fields:
     *  - Word 0: general configuration: bit 7 = removable, bit 15 = fixed disk
     *  - Word 1, 3, 6: number of cylinders, heads, sectors (for legacy CHS addressing)
     *  - Word 10-19: serial number (20 ASCII characters, right-padded with spaces)
     *  - Word 23-26: firmware revision (8 ASCII characters, right-padded with spaces)
     *  - Word 27-46: model number (40 ASCII characters, right-padded with spaces)
     *  - Word 47: maximum block transfer size in (512-byte?) sectors
     *  - Word 49, 51: capabilities
     *  - Word 60-61: total number of user addressable sectors (LBA28)
     *  - Word 80-82: ultra DMA modes supported/enabled (for high-speed transfers)
     *  - Word 82: supported major version
     *  - Word 83: command set support (ata-7/ata-8, NCQ, SMART, security features, etc.)
     *     - Bit 10: supports 48-bit addressing
     *  - Word 88-93:
     *  - Word 100-103: total number of user addressable sectors (LBA48)
     *  - Word 106: physical sector size / logical sector size
     *     - Bit 0-3: 2^x logical sectors per physical sector (0 = 1 sector, 1 = 2 sectors, etc.). This has
     *       performance implications as unaligned writes are slower, despite being aligned to logical sectors.
     *     - Bit 12: logical sector longer than 256 words
     *     - Bit 13: device has multiple logical sectors per physical sector
     *  - Word 117-118: words per logical sector (sometimes 2 for 512-byte emulation of 4K disks); valid if bit
     *    12 of word 106 is set. This is a DWORD (32 bit); word 117 is the low word, 118 is the high word.
     */

    // Sector size
    dev->logical_sector_size = (uint32_t)512; // Default
    if (dev->identify.sector_size_info & 0x1000) {
        dev->logical_sector_size = ((struct ata_identify *)&dev->identify)->logical_sector_words * 2;
    }

    dev->physical_sector_size = dev->logical_sector_size;
    if (dev->identify.sector_size_info & 0x000F) {
        dev->physical_sector_size = dev->logical_sector_size * (1 << (dev->identify.sector_size_info & 0x000F));
    }

    // Number of sectors
    if (dev->identify.command_set_support & (1 << 10)) {
        // 48-bit LBA supported
        dev->numsectors = dev->identify.sectors48;

        dev->flags |= IDE_DEVICE_FLAG_LBA48;
    } else {
        // 28-bit LBA or legacy CHS
        dev->numsectors = dev->identify.sectors28;
    }

    return 0;
}