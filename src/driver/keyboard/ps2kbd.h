#pragma once

#include "plenjos/dev/keycodes.h"

/* Registers */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

/**
 * PS/2 controller configuration byte bits
 * (Bit 0 = smallest bit)
 * Bit 0: IRQ1 enable (port 1)
 * Bit 1: IRQ12 enable (port 2)
 * Bit 2: System flag
 * Bit 3: Zero (should be 0)
 * Bit 4: First PS/2 port clock disable
 * Bit 5: Second PS/2 port clock disable
 * Bit 6: Translation (set to 1 to translate Set 2 to Set 1)
 * Bit 7: Zero (should be 0)
 */
#define PS2_CFG_IRQ1_ENABLE       0x01
#define PS2_CFG_IRQ12_ENABLE      0x02
#define PS2_CFG_DISABLE_PORT1_CLK 0x10
#define PS2_CFG_DISABLE_PORT2_CLK 0x20
#define PS2_CFG_TRANSLATION       0x40

/**
 * PS/2 status register values:
 * (Bit 0 = smallest bit)
 * Bit 0: output buffer status
 * Bit 1: input buffer status
 * Bit 2: system flag
 * Bit 3: command/data (0 = data written to bus is for PS/2 device data;
 *                      1 = data written to bus is for PS/2 controller command)
 * Bit 4: unknown / unused
 * Bit 5: unknown / unused
 * Bit 6: time-out error
 * Bit 7: parity error
 */
#define PS2_OBF 0x01 // Output buffer full
#define PS2_IBF 0x02 // Input buffer full

/**
 * PS/2 Controller Commands
 */
#define PS2_READ_CONFIG   0x20
#define PS2_WRITE_CONFIG  0x60
#define PS2_DISABLE_PORT1 0xAD
#define PS2_ENABLE_PORT1  0xAE
#define PS2_DISABLE_PORT2 0xA7
#define PS2_ENABLE_PORT2  0xA8
#define PS2_SELF_TEST     0xAA
#define PS2_TEST_PORT1    0xAB
#define PS2_TEST_PORT2    0xA9
#define PS2_WRITE_PORT2   0xD4

#define PS2_SELF_TEST_OK 0x55

/**
 * PS/2 Device commands
 */
#define PS2_DEV_RESET  0xFF
#define PS2_DEV_ENABLE 0xF4

/**
 * Responses
 */
#define PS2_ACK         0xFA
#define PS2_SELFTEST_OK 0xAA

/**
 * Advanced scancode info
 */
#define PS2_EXTENDED_SCANCODE_PREFIX 0xE0 // Extended keys; read 1 more byte
#define PS2_RELEASE_PREFIX           0xF0 // Key release; next byte is the key released

/**
 * Pause/break key sequence: (PS/2 Set 2)
 * E1 14 77 E1 F0 14 F0 77
 *
 * This is a special case; it doesn't follow the usual make/break code pattern. It never releases; don't wait for a
 * release.
 */
#define PS2_PAUSE_PREFIX  0xE1 // Pause/break; read the entire sequence as defined below
#define PS2_PAUSE_SEQ_DEF { 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77 }

/* Maps Set 2 scancode 0x00–0x7F to keycode_t */
extern const uint16_t _PS2_NORMAL_LOOKUP[256];
extern const uint16_t _PS2_EXTENDED_LOOKUP[256];
extern const uint8_t PS2_PAUSE_SEQ[8];

// Scan codes copied from KeblaOS
// Scan Code values
#define PS2_UNKNOWN 0xFFFFFFFF

#define PS2_ESC             0x00000001
#define PS2_1               0x00000002
#define PS2_2               0x00000003
#define PS2_3               0x00000004
#define PS2_4               0x00000005
#define PS2_5               0x00000006
#define PS2_6               0x00000007
#define PS2_7               0x00000008
#define PS2_8               0x00000009
#define PS2_9               0x0000000A
#define PS2_0               0x0000000B
#define PS2_MINUS           0x0000000C // '-'
#define PS2_EQUAL           0x0000000D // '='
#define PS2_BACKSPACE       0x0000000E
#define PS2_TAB             0x0000000F
#define PS2_Q               0x00000010
#define PS2_W               0x00000011
#define PS2_E               0x00000012
#define PS2_R               0x00000013
#define PS2_T               0x00000014
#define PS2_Y               0x00000015
#define PS2_U               0x00000016
#define PS2_I               0x00000017
#define PS2_O               0x00000018
#define PS2_P               0x00000019
#define PS2_LBRACKET        0x0000001A // '['
#define PS2_RBRACKET        0x0000001B // ']'
#define PS2_ENTER           0x0000001C
#define PS2_CTRL            0x0000001D
#define PS2_A               0x0000001E
#define PS2_S               0x0000001F
#define PS2_D               0x00000020
#define PS2_F               0x00000021
#define PS2_G               0x00000022
#define PS2_H               0x00000023
#define PS2_J               0x00000024
#define PS2_K               0x00000025
#define PS2_L               0x00000026
#define PS2_SEMICOLON       0x00000027 // ''
#define PS2_APOSTROPHE      0x00000028 // '\''
#define PS2_BACKTICK        0x00000029 // '`'
#define PS2_LSHIFT          0x0000002A
#define PS2_BACKSLASH       0x0000002B // '\'
#define PS2_Z               0x0000002C
#define PS2_X               0x0000002D
#define PS2_C               0x0000002E
#define PS2_V               0x0000002F
#define PS2_B               0x00000030
#define PS2_N               0x00000031
#define PS2_M               0x00000032
#define PS2_COMMA           0x00000033 // ','
#define PS2_PERIOD          0x00000034 // '.'
#define PS2_SLASH           0x00000035 // '/'
#define PS2_RSHIFT          0x00000036
#define PS2_NUMPAD_ASTERISK 0x00000037 // '*'
#define PS2_ALT             0x00000038
#define PS2_SPACE           0x00000039
#define PS2_CAPS_LOCK       0x0000003A

#define PS2_F1          0x0000003B
#define PS2_F2          0x0000003C
#define PS2_F3          0x0000003D
#define PS2_F4          0x0000003E
#define PS2_F5          0x0000003F
#define PS2_F6          0x00000040
#define PS2_F7          0x00000041
#define PS2_F8          0x00000042
#define PS2_F9          0x00000043
#define PS2_F10         0x00000044
#define PS2_NUM_LOCK    0x00000045
#define PS2_SCROLL_LOCK 0x00000046

#define PS2_NUMPAD_7      0x00000047
#define PS2_NUMPAD_8      0x00000048
#define PS2_NUMPAD_9      0x00000049
#define PS2_NUMPAD_MINUS  0x0000004A
#define PS2_NUMPAD_4      0x0000004B
#define PS2_NUMPAD_5      0x0000004C
#define PS2_NUMPAD_6      0x0000004D
#define PS2_NUMPAD_PLUS   0x0000004E
#define PS2_NUMPAD_1      0x0000004F
#define PS2_NUMPAD_2      0x00000050
#define PS2_NUMPAD_3      0x00000051
#define PS2_NUMPAD_0      0x00000052
#define PS2_NUMPAD_PERIOD 0x00000053 // '.'

#define PS2_F11 0x00000057
#define PS2_F12 0x00000058

#define PS2_HOME      0x00000047 // Same as NUMPAD_7 without Num Lock
#define PS2_UP        0x00000048 // Same as NUMPAD_8 without Num Lock
#define PS2_PAGE_UP   0x00000049 // Same as NUMPAD_9 without Num Lock
#define PS2_LEFT      0x0000004B // Same as NUMPAD_4 without Num Lock
#define PS2_RIGHT     0x0000004D // Same as NUMPAD_6 without Num Lock
#define PS2_END       0x0000004F // Same as NUMPAD_1 without Num Lock
#define PS2_DOWN      0x00000050 // Same as NUMPAD_2 without Num Lock
#define PS2_PAGE_DOWN 0x00000051 // Same as NUMPAD_3 without Num Lock
#define PS2_INSERT    0x00000052 // Same as NUMPAD_0 without Num Lock
#define PS2_DELETE    0x00000053 // Same as NUMPAD_PERIOD without Num Lock

int init_ps2_keyboard();