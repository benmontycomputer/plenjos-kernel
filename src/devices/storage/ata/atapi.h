#pragma once

#include <stddef.h>
#include <stdint.h>

extern struct ide_device;

/**
 * ATAPI command codes.
 *
 * Command sequence for a minimal driver:
 *  1. IDENTIFY PACKET command (handled in ide.c)
 *  2. INQUIRY command to identify device type
 *  3. TEST UNIT READY to check if media is present (loop until ready or failure)
 *  4. REQUEST SENSE if TEST UNIT READY fails, to get error details
 *  5. READ CAPACITY (10) to get last LBA and block size
 *  6. READ (10) to read data blocks
 */

// Identify device type and capabilities
#define ATAPI_COMMAND_INQUIRY 0x12 // 12h 00h 00h 00h 24h 00h 00h 00h 00h 00h 00h 00h

// Check if media is present and ready
#define ATAPI_COMMAND_TEST_UNIT_READY 0x00 // 00h 00h 00h 00h 00h 00h

// Retrieve error details after failure
#define ATAPI_COMMAND_REQUEST_SENSE 0x03 // 03h 00h 00h 00h 12h 00h

// Returns last LBA address and block size
#define ATAPI_COMMAND_READ_CAPACITY_10 0x25 // 25h 00h 00h 00h 00h 00h 00h 00h 08h 00h

// Read data blocks from the disc
#define ATAPI_COMMAND_READ_10 0x28 // 28h 00h lba[4] 00h count[2] 00h

// Spin up / spin down disc (required for some drives)
#define ATAPI_COMMAND_START_STOP_UNIT 0x1B // 1Bh 00h 00h 00h 00h 00h

// Read drive parameters (rarely needed)
#define ATAPI_COMMAND_MODE_SENSE_10 0x5A // 5Ah 00h 00h 00h 00h 00h 00h 00h 20h 00h

// Make sure to select the device and lock the bus first!
int atapi_send_packet_command(struct ide_device *dev, const uint8_t *packet, size_t packet_len, void *buffer,
                              size_t buffer_len, int is_write, int is_dma);

// https://web.archive.org/web/20221119212108/https://node1.123dok.com/dt01pdf/123dok_us/001/139/1139315.pdf.pdf?X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=7PKKQ3DUV8RG19BL%2F20221119%2F%2Fs3%2Faws4_request&X-Amz-Date=20221119T211954Z&X-Amz-SignedHeaders=host&X-Amz-Expires=600&X-Amz-Signature=c4154afd7a42db51d05db14370b8e885c3354c14cb66986bffcfd1a9a0869ee2
struct atapi_identify {
    /**
     * Word 0
     *
     * General config info
     *  - Bits 15-14: device type (10 = ATAPI device; 11 = reserveed)
     *  - Bit 13: reserved
     *  - Bit 12-8: command packet set supported by device (0x1h = sequential access device)
     *  - Bit 7: removable media
     *  - Bits 6-5: method device uses when ready for Host to xfer packet data
     *     - 00 = Microprocessor DRQ (device shall set DRQ=1 within 3ms of receiving PACKET command)
     *     - 01 = Interrupt DRQ
     *     - 10 = Accelerated DRQ (within 50us of PACKET command)
     *     - 11 = reserved
     *  - Bits 4-2: reserved
     *  - Bits 1-0: command packet size (00 = 12 bytes; 01 = 16 bytes; 1X=reserved)
     */
    uint16_t general_config;

    // Words 1-9: reserved
    uint16_t reserved1[9];

    // Words 10–19: serial number
    char serial[20]; // ASCII, right-padded with spaces

    // Words 20–22: reserved
    uint16_t reserved2[3];

    // Words 23–26: firmware revision
    char firmware[8]; // ASCII, right-padded

    // Words 27–46: model number
    char model[40]; // ASCII, right-padded

    // Words 47-48: reserved
    uint16_t reserved3[2];

    /**
     * Word 49: Capabilities
     *  - Bit 15: interleaved DMA support
     *  - Bit 14: command queuing
     *  - Bit 13: overlap operation
     *  - Bit 12: ATA software reset required (obsolete)
     *  - Bit 11: IORDY supported (must be set if device supports PIO mode 3 or higher)
     *  - Bit 10: IORDY may be disabled (via SET FEATURES command)
     *  - Bit 9: LBA supported
     *  - Bit 8: 0 = DMA supported; 1 = DMA not supported
     *  - Bits 7-0: vendor specific
     */
    uint16_t capabilities;

    // Word 50: reserved
    uint16_t reserved4;

    /**
     * Word 51: PIO data transfer mode number
     *  - Bits 15-8: 0x00 = mode 0; 0x01 = mode 1; 0x02 = mode 2; 0x03-0xFF reserved
     *  - Bits 7-0: vendor specific
     */
    uint16_t pio_mode;

    // Word 52: reserved
    uint16_t reserved5;

    /**
     * Word 53: indicates which optional words are valid
     *  - Bits 15-3: reseerved
     *  - Bit 2: word 88 valid
     *  - Bit 1: words 64-70 valid
     *  - Bit 0: words 54-58 valid
     */
    uint16_t optional_words_valid;

    // Reserved for ATA devices
    uint16_t reserved_for_ata[9];

    /** Word 63: multimode DMA mode selected/supported. Only 1 mode can be selected at a time. If an UltraDMA mode is
     * selected, bits 15-8 must be 0.
     *  - Bits 15-11: reserved
     *  - Bit 10: multimode DMA mode 2 selected
     *  - Bit 9: multimode DMA mode 1 selected
     *  - Bit 8: multimode DMA mode 0 selected
     *  - Bits 7-3: reserved
     *  - Bit 2: multimode DMA mode 2 supported
     *  - Bit 1: multimode DMA mode 1 supported
     *  - Bit 0: multimode DMA mode 0 supported
     */
    uint16_t multi_dma_mode;

    /**
     * Word 64: advanced PIO transfer modes supported (15-2 reserved, bit 1 = PIO mode 4 supported, bit 0 = PIO mode 3
     * supported). Check word 53 to see if this word is valid.
     */
    uint16_t advanced_pio_modes;

    /**
     * Word 65: minimum MDMA transfer cycle time per word (in nanoseconds). Check word 53 to see if this word is valid.
     *  - MDMA mode 2: 120=0078h
     *  - MDMA mode 1: 150=0096h
     *  - MDMA mode 0: 480=01E0h
     */
    uint16_t min_mdma_cycle_time;

    /** Word 66: manufacturer recommended MDMA transfer cycle time per word (in nanoseconds). Check word 53 to see if
     * this word is valid.
     */
    uint16_t rec_mdma_cycle_time;

    /**
     * Word 67: minimum PIO transfer cycle time without flow control (in nanoseconds). Check word 53 to see if this word
     * is valid.
     *  - PIO mode 4: 120=0078h
     *  - PIO mode 3: 180=00B4h
     *  - PIO mode 2: 240=00F0h
     *  - PIO mode 1: 383=017Fh
     *  - PIO mode 0: 600=0258h
     */
    uint16_t min_pio_cycle_time_no_flow;

    /** Word 68: minimum PIO transfer cycle time with IORDY flow control (in nanoseconds). Check word 53 to see if this
     * word is valid.
     */
    uint16_t min_pio_cycle_time_with_flow;

    // Words 69-70: reserved
    uint16_t reserved6[2];

    /**
     * Words 71-72: not supported (timing stuff). TODO: implement if needed
     */
    uint16_t not_supported[2];

    // Words 73-74: reserved
    uint16_t reserved7[2];

    // Word 75: not supported (queue depth) (bits 4-0). Todo: implement if needed
    uint16_t not_supported_queue_depth;

    // Words 76-79: reserved
    uint16_t reserved8[4];

    /**
     * Word 80: major version number.
     *  - Bits 15-5: reserved
     *  - Bit 4: ATA/ATAPI-4 supported
     *  - Bit 3: ATA-3 supported
     *  - Bit 2: ATA-2 supported
     *  - Bit 1: ATA-1 supported
     *  - Bit 0: reserved
     */
    uint16_t major_version;

    /**
     * Word 81: minor version number.
     *  - 000Fh = ATA/ATAPI-4 T13 1153D revision 7
     */
    uint16_t minor_version;

    /**
     * Words 82-83: command set support. If words 82 and 83 = 0000h or FFFFh, then command set notification is not
     * supported.
     *
     * Word 82:
     *  - Bit 15: IDENTIFY DEVICE DMA command supported
     *  - Bit 14: NOP command supported
     *  - Bit 13: READ BUFFER command supported
     *  - Bit 12: WRITE BUFFER command supported
     *  - Bit 11: WRITE VERIFY command supported
     *  - Bit 10: Host protected area feature set supported
     *  - Bit 9: DEVICE RESET command supported
     *  - Bit 8: SERVICE interrupt supported
     *  - Bit 7: Release interrupt supported
     *  - Bit 6: Look-ahead supported
     *  - Bit 5: write cache supported
     *  - Bit 4: PACKET command feature set supported
     *  - Bit 3: Power management feature set supported
     *  - Bit 2: Removable media feature set supported
     *  - Bit 1: Security mode feature set supported
     *  - Bit 0: SMART feature set supported
     *
     * Word 83:
     *  - Bit 15: shall be cleared to 0
     *  - Bit 14: shall be set to one
     *  - Bits 13-3: reserved
     *  - Bit 2: Compact flash feature set supported
     *  - Bit 1: Read/write DMA QUEUED command supported
     *  - Bit 0: DOWNLOAD MICROCODE command supported
     */
    union {
        struct {
            uint16_t identify_device_dma  : 1;
            uint16_t nop_command          : 1;
            uint16_t read_buffer_command  : 1;
            uint16_t write_buffer_command : 1;
            uint16_t write_verify_command : 1;
            uint16_t host_protected_area  : 1;
            uint16_t device_reset_command : 1;
            uint16_t service_interrupt    : 1;
            uint16_t release_interrupt    : 1;
            uint16_t look_ahead           : 1;
            uint16_t write_cache          : 1;
            uint16_t packet_command_set   : 1;
            uint16_t power_management     : 1;
            uint16_t removable_media      : 1;
            uint16_t security_mode        : 1;
            uint16_t smart                : 1;

            uint16_t reserved1          : 1;
            uint16_t must_be_one        : 1;
            uint16_t reserved2          : 11;
            uint16_t compact_flash      : 1;
            uint16_t rw_dma_queued      : 1;
            uint16_t download_microcode : 1;
        };
        uint16_t raw[2];
    } command_feature_set_support;

    /**
     * Word 84: Command/feature set supported extension. If words 82, 83, and 84 = 0000h or FFFFh, then command set
     * notification extension is not supported.
     *  - Bit 15: shall be cleared to 0
     *  - Bit 14: shall be set to one
     *  - Bits 13-0: reserved
     */
    uint16_t command_feature_set_support_ext;

    /**
     * Words 85-86: Command/feature set enabled. If words 82 and 83 = 0000h or FFFFh, then command set notification is
     * not supported.
     *
     * Word 85:
     *  - Bit 15: IDENTIFY DEVICE DMA command enabled
     *  - Bit 14: NOP command enabled
     *  - Bit 13: READ BUFFER command enabled
     *  - Bit 12: WRITE BUFFER command enabled
     *  - Bit 11: WRITE VERIFY command enabled
     *  - Bit 10: Host protected area feature set enabled
     *  - Bit 9: DEVICE RESET command enabled
     *  - Bit 8: SERVICE interrupt enabled
     *  - Bit 7: Release interrupt enabled
     *  - Bit 6: Look-ahead enabled
     *  - Bit 5: write cache enabled
     *  - Bit 4: PACKET command feature set enabled
     *  - Bit 3: Power management feature set enabled
     *  - Bit 2: Removable media feature set enabled
     *  - Bit 1: Security mode feature set enabled
     *  - Bit 0: SMART feature set enabled
     *
     * Word 86:
     *  - Bit 15: shall be cleared to 0
     *  - Bit 14: shall be set to one
     *  - Bits 13-3: reserved
     *  - Bit 2: Compact Flash feature set enabled
     *  - Bit 1: Read/write DMA QUEUED command enabled
     *  - Bit 0: DOWNLOAD MICROCODE command enabled
     */
    union {
        struct {
            uint16_t identify_device_dma  : 1;
            uint16_t nop_command          : 1;
            uint16_t read_buffer_command  : 1;
            uint16_t write_buffer_command : 1;
            uint16_t write_verify_command : 1;
            uint16_t host_protected_area  : 1;
            uint16_t device_reset_command : 1;
            uint16_t service_interrupt    : 1;
            uint16_t release_interrupt    : 1;
            uint16_t look_ahead           : 1;
            uint16_t write_cache          : 1;
            uint16_t packet_command_set   : 1;
            uint16_t power_management     : 1;
            uint16_t removable_media      : 1;
            uint16_t security_mode        : 1;
            uint16_t smart                : 1;
            uint16_t reserved1            : 1;
            uint16_t must_be_one          : 1;
            uint16_t reserved2            : 11;
            uint16_t compact_flash        : 1;
            uint16_t rw_dma_queued        : 1;
            uint16_t download_microcode   : 1;
        };
        uint16_t raw[2];
    } command_feature_set_enabled;

    /**
     * Word 87: Command/feature set enabled default. If words 82, 83, and 84 = 0000h or FFFFh, then command set
     * notification extension is not supported.
     *  - Bit 15: shall be cleared to 0
     *  - Bit 14: shall be set to one
     *  - Bits 13-0: reserved
     */
    uint16_t command_feature_set_enabled_default;

    /**
     * Word 88: Supported and selected UltraDMA modes.
     *  - Bits 15-11: reserved
     *  - Bit 10: UltraDMA mode 2 selected
     *  - Bit 9: UltraDMA mode 1 selected
     *  - Bit 8: UltraDMA mode 0 selected
     *  - Bits 7-3: reserved
     *  - Bit 2: UltraDMA mode 2 and below supported
     *  - Bit 1: UltraDMA mode 1 and below supported
     *  - Bit 0: UltraDMA mode 0 supported
     */
    uint16_t ultra_dma_modes;

    // Word 89: time required for security erase unit completion
    uint16_t security_erase_time;

    // Word 90: time required for enhanced security erase unit completion
    uint16_t enhanced_security_erase_time;

    // Word 91: current advanced power management value
    uint16_t current_apm_value;

    // Word 92: master password revision code
    uint16_t master_password_revision;

    /**
     * Word 93: hardware reset result. This shall change only during the execution of a hardware reset.
     *  - Bit 15: shall be cleared to 0
     *  - Bit 14: shall be set to one
     *  - Bit 13: 1 = device detected CBLID - above ViH; 0 = device detected CBLID - below ViL
     *  - Bits 12-8: Device 1 hardware reset result. Device 0 shall clear these bits to 0. Device 1 shall set these bits
     * as follows:
     *     - Bit 12: Reserved
     *     - Bit 11: Device 1 asserted PDIAG-
     *     - Bits 10-9: How Device 1 determined the device number:
     *        - 00 = Reserved
     *        - 01 = a jumper was used
     *        - 10 = the CSEL signal was used
     *        - 11 = some other method was used or the method is unknown
     *     - Bit 8: shall be set to one.
     *  - Device 0 hardware reset result. Device 1 shall clear these bits to zero. Device 0 shall set these bits as
     * follows:
     *     - Bit 7: Reserved
     *     - Bit 6: Whether Device 0 responds when Device 1 is selected
     *     - Bit 5: Device 0 detected the assertion of DASP-
     *     - Bit 4: Device 0 detected the assertion of PDIAG-
     *     - Bit 3: Device 0 passed diagnostics
     *     - Bits 2-1: How Device 0 determined the device number:
     *        - 00 = Reserved
     *        - 01 = a jumper was used
     *        - 10 = the CSEL signal was used
     *        - 11 = some other method was used or the method is unknown
     *     - Bit 0: shall be set to one.
     */
    uint16_t hardware_reset_result;

    // Bits 94-126: reserved
    uint16_t reserved9[33];

    /**
     * Word 127: Removable media status notification feature set support.
     *  - Bits 15-2: reserved
     *  - Bits 1-0:
     *     - 00 = not supported
     *     - 01 = supported (with polling?)
     *     - 10 = reserved
     *     - 11 = reserved
     */
    uint16_t removable_media_status_notification;

    /**
     * Word 128: Security status.
     *  - Bits 15-9: reserved
     *  - Bit 8: security level (0 = high; 1 = maximum)
     *  - Bits 7-6: reserved
     *  - Bit 5: enhanced security erase supported
     *  - Bit 4: security count expired
     *  - Bit 3: security frozen
     *  - Bit 2: security locked
     *  - Bit 1: security enabled
     *  - Bit 0: security supported
     */
    uint16_t security_status;

    // Words 129-159: vendor specific
    uint16_t vendor_specific[31];

    /**
     * Word 160: CFA power mode 1
     *  - Bit 15: word 160 supported
     *  - Bit 14: reserved
     *  - Bit 13: CFA power mode 1 is required for one or more commands implemented by the device
     *  - Bit 12: CFA power mode 1 disabled
     *  - Bits 11-0: maximum current in mA
     */
    uint16_t cfa_power_mode1;

    // Words 161-175: reserved for assignment by CompactFlash Association
    uint16_t reserved_for_cfa[15];

    // Words 176-254: reserved
    uint16_t reserved10[79];

    // Word 255: integrity word (15-8 = checksum; 7-0 = signature)
    uint16_t integrity_word;
};