#pragma once

#include "ata.h"
#include "devices/storage/drive.h"
#include "kernel.h"

#include <stddef.h>
#include <stdint.h>

extern struct ide_device;

struct pata_identify {
    // Word 0
    uint16_t general_config;

    // Words 1–3
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;

    // Word 4: number of heads
    uint16_t num_heads;

    // Word 5: number of sectors per track
    uint16_t sectors_per_track2;

    // Word 6: number of cylinders
    uint16_t num_cylinders;

    // Word 7: number of sectors per track
    uint16_t sectors_per_track3;

    // Word 8: number of heads
    uint16_t num_heads2;

    // Word 9: reserved
    uint16_t reserved;

    // Words 10–19: serial number
    char serial[20]; // ASCII, right-padded with spaces

    // Words 20–22: reserved
    uint16_t reserved1[3];

    // Words 23–26: firmware revision
    char firmware[8]; // ASCII, right-padded

    // Words 27–46: model number
    char model[40]; // ASCII, right-padded

    // Words 47–48: capabilities, multi-sector
    uint16_t max_multi_sector;
    uint16_t capabilities;

    // Word 49: reserved or capabilities
    uint16_t reserved2;

    // Word 50–51: total user-addressable sectors (28-bit LBA)
    union {
        struct {
            uint16_t sectors28_lo;
            uint16_t sectors28_hi;
        };
        uint32_t sectors28;
    };

    // Words 52–81: reserved
    uint16_t reserved3[30];

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
    uint16_t reserved4[2];

    // Word 106: physical/logical sector size info
    uint16_t sector_size_info;

    // Words 107–116: reserved
    uint16_t reserved5[10];

    // Words 117–118: words per logical sector
    union {
        struct {
            uint16_t logical_sector_words_lo;
            uint16_t logical_sector_words_hi;
        };
        uint32_t logical_sector_words;
    };

    // Remaining words are reserved
    uint16_t reserved6[256 - 119];
} __attribute__((packed));

struct pata_info {
    // PATA specific info can go here
};