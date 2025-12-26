#pragma once

#include "devices/storage/drive.h"

#include <stddef.h>
#include <stdint.h>

struct iso9660_volume_descriptor_generic {
    uint8_t type;
    char id[5];
    uint8_t version;

    uint8_t data[2041];
} __attribute__((packed));

struct iso9660_boot_record_volume_descriptor {
    uint8_t type;    // 0x00
    char id[5];      // "CD001"
    uint8_t version; // 0x01

    uint8_t unused[2041];
} __attribute__((packed));

struct iso9660_primary_volume_descriptor {
    uint8_t type;    // 0x01
    char id[5];      // "CD001"
    uint8_t version; // 0x01

    uint8_t unused1;
    char system_identifier[32];
    char volume_identifier[32];
    uint8_t unused2[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;

    uint8_t unused3[32];
    uint16_t volume_set_size_le;
    uint16_t volume_set_size_be;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint16_t logical_block_size_le;
    uint16_t logical_block_size_be;
    uint32_t path_table_size_le;
    uint32_t path_table_size_be;
    uint32_t type_l_path_table_location;
    uint32_t optional_type_l_path_table_location;
    uint32_t type_m_path_table_location;
    uint32_t optional_type_m_path_table_location;

    uint8_t root_directory_record[34];

    char volume_set_identifier[128];
    char publisher_identifier[128];
    char data_preparer_identifier[128];
    char application_identifier[128];
    char copyright_file_identifier[37];
    char abstract_file_identifier[37];
    char bibliographic_file_identifier[37];

    uint8_t creation_date[17];
    uint8_t modification_date[17];
    uint8_t expiration_date[17];
    uint8_t effective_date[17];

    uint8_t file_structure_version;
    uint8_t unused4;
    uint8_t application_used[512];
    uint8_t reserved[653];
} __attribute__((packed));

struct iso9660_volume_descriptor_set_terminator {
    uint8_t type;    // 0xFF
    char id[5];      // "CD001"
    uint8_t version; // 0x01

    uint8_t unused[2041];
} __attribute__((packed));

/**
 * ISO9660 Path Table Entry. Unlike directory records, path table entries are allowed to cross sector boundaries.
 * However, they must always start at an even address.
 *
 * Path tables are ordered in ascending order of directory level and are alphabetical within each level.
 */
struct iso9660_path_table_entry {
    uint8_t directory_identifier_length;
    uint8_t extended_attribute_record_length;
    uint8_t extent_location_data[4];         // This can be LE or BE based on context
    uint8_t parent_directory_number_data[2]; // This can be LE or BE based on context
    char directory_identifier[];             // Variable length
    // Padding to even length follows (if the struct would have odd length, a single padding byte is added)
} __attribute__((packed));

/**
 * ISO9660 Directory Record. Unlike path table entries, directory records must not cross sector boundaries.
 */
struct iso9660_directory_record {
    uint8_t length; // Length of directory record
    uint8_t extended_attribute_record_length;
    uint32_t extent_location_lba_le;
    uint32_t extent_location_lba_be;

    uint32_t data_length_le;
    uint32_t data_length_be;

    /**
     * Different from date/time format used in primary volume descriptor.
     *
     * From offset 0 to 6:
     *   Year since 1900 (1 byte)
     *   Month (1 byte)
     *   Day (1 byte)
     *   Hour (1 byte)
     *   Minute (1 byte)
     *   Second (1 byte)
     *   GMT offset from -48 (west) to +52 (east) in 15-minute intervals (1 byte)
     */
    uint8_t date_time[7];

    /**
     * File flags:
     * Bit 0: Existence (0 = exists, 1 = deleted/hidden)
     * Bit 1: Directory (0 = file, 1 = directory)
     * Bit 2: Associated file (0 = normal file, 1 = associated file)
     * Bit 3: Record (whether the extended attribute record is present)
     * Bit 4: Protection (whether owner and group are set in the extended attribute record)
     * Bits 5-6: reserved
     * Bit 7: Multi-extent (if set, this is not the final extent of the file; used for files spanning multiple extents)
     */
    uint8_t file_flags;
    uint8_t file_unit_size;      // For records in interleaved mode; zero otherwise
    uint8_t interleave_gap_size; // For records in interleaved mode; zero otherwise
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint8_t file_name_length;
    char file_identifier[]; // Variable length
    // Padding to even length follows (if the struct would have odd length, a single padding byte is added)
    // System Use Area follows (variable length; can extend up to max record length of 255 bytes)
} __attribute__((packed));

struct filesystem_iso9660 {
    DRIVE_t *drive;
    uint32_t partition_start_lba;

    uint32_t logical_block_size; // From primary volume descriptor

    uint8_t read_status; // Bitfield: 0x01 = primary volume descriptor read
    struct iso9660_primary_volume_descriptor pvd;

    uint32_t path_table_size;
    uint32_t type_l_path_table_location;
    uint32_t type_m_path_table_location;

    uint32_t root_directory_extent_location;
    uint32_t root_directory_size;
};

int iso9660_setup(struct filesystem_iso9660 *fs, DRIVE_t *drive, uint32_t partition_start_lba);

typedef struct vfs_iso9660_cache_node_data {
    struct filesystem_iso9660 *fs;
    struct iso9660_directory_record *dir_record;
    uint64_t unused[2];
} __attribute__((packed)) vfs_iso9660_cache_node_data_t;

typedef struct vfs_iso9660_handle_instance_data {
    // struct filesystem_iso9660 *fs; // This can be found from the cache node data
    // struct iso9660_directory_record dir_record; // This can be found from the cache node data
    uint64_t current_extent_location; // Current extent location being read (lba offset)
    uint64_t extent_pos;              // Offset within current extent
    uint64_t seek_pos;
    uint64_t unused;
} __attribute__((packed)) vfs_iso9660_handle_instance_data_t;