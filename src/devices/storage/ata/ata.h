#pragma once

#include "atapi.h"

#include <stddef.h>
#include <stdint.h>

extern struct ide_device;

/**
 * ATA registers (offsets from command block base)
 */
#define ATA_REG_DATA     0
#define ATA_REG_ERROR    1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA0     3
#define ATA_REG_LBA1     4
#define ATA_REG_LBA2     5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_STATUS   7
#define ATA_REG_COMMAND  7

#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

#define ATA_CMD_PACKET 0xA0

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF  0x20
#define ATA_SR_ERR 0x01

enum ata_device_type {
    ATADEV_NONE   = 0,
    ATADEV_PATA   = 1,
    ATADEV_PATAPI = 2,
    ATADEV_SATA   = 3,
    ATADEV_SATAPI = 4,
};

struct ata_identify {
    // Word 0
    uint16_t general_config;

    // Words 1–3
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;

    // Word 4–9: reserved
    uint16_t reserved1[6];

    // Words 10–19: serial number
    char serial[20]; // ASCII, right-padded with spaces

    // Words 20–22: reserved
    uint16_t reserved2[3];

    // Words 23–26: firmware revision
    char firmware[8]; // ASCII, right-padded

    // Words 27–46: model number
    char model[40]; // ASCII, right-padded

    // Words 47–48: capabilities, multi-sector
    uint16_t max_multi_sector;
    uint16_t capabilities;

    // Word 49: reserved or capabilities
    uint16_t reserved3;

    // Word 50–51: total user-addressable sectors (28-bit LBA)
    union {
        struct {
            uint16_t sectors28_lo;
            uint16_t sectors28_hi;
        };
        uint32_t sectors28;
    };

    // Words 52–81: reserved
    uint16_t reserved4[30];

    // Words 82-83: major version, command set support
    uint16_t major_version;
    uint16_t command_set_support;

    // Word 84: command set support extensions
    uint16_t command_set_ext;

    // Word 85: command set enabled
    uint16_t command_set_enable;

    // Words 86-99: reserved
    uint16_t reserved_for_exp[14];

    // Words 100–103: total sectors 48-bit LBA
    union {
        struct {
            uint16_t sectors48_lo;
            uint16_t sectors48_mid_lo;
            uint16_t sectors48_mid_hi;
            uint16_t sectors48_hi;
        };
        uint64_t sectors48;
    };

    // Words 104-105: reserved
    uint16_t reserved5[2];

    // Word 106: physical/logical sector size info
    uint16_t sector_size_info;

    // Words 107–116: reserved
    uint16_t reserved6[10];

    // Words 117–118: words per logical sector
    union {
        struct {
            uint16_t logical_sector_words_lo;
            uint16_t logical_sector_words_hi;
        };
        uint32_t logical_sector_words;
    };

    // Remaining words are reserved
    uint16_t reserved7[256 - 119];
} __attribute__((packed));

struct ata_info {
    // ATA specific info can go here
};

struct atapi_info {
    // ATAPI specific info can go here
};

void ata_parse_identify(struct ide_device *dev);
void atapi_parse_identify(struct ide_device *dev);

void atapi_read_data(struct ide_device *dev, void *buffer, size_t bytes);
void atapi_write_data(struct ide_device *dev, const void *buffer, size_t bytes);

void atapi_ready_device(struct ide_device *dev);

int ata_wait_bsy_read_status(uint16_t io);
int ata_wait_drq(uint16_t io);