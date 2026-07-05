#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "keycodes.h"

#define KBD_BUFFER_SIZE 128

typedef enum {
    KEY_RELEASED = 0,
    KEY_PRESSED  = 1,
    KEY_REPEAT   = 2,
} kbd_key_state;

typedef struct {
    enum key_codes code : 16;
    uint8_t state;
    uint8_t mods; // Useful if userspace doesn't want to keep track of modifier states
} __attribute__((packed)) kbd_event_t;

/**
 * Checks if a scancode corresponds to a character affected by caps lock.
 */
static inline bool _kbdev_is_caps_affected(uint16_t scan) {
    return (scan >= KEY_A && scan <= KEY_Z);
}

/**
 * Checks if a scancode corresponds to a printable character.
 *
 * These include letters, numbers, space bar, and common symbols. Notably, they do NOT include tab, return, or backspace.
 */
static inline bool _kbdev_is_printable(uint16_t scan) {
    return (scan >= KEY_A && scan <= KEY_0) || (scan >= KEY_BACKTICK && scan <= KEY_SPACE);
}

static inline bool _ascii_is_printable(char ch) {
    return ch >= ' ' && ch <= '~';
}

/**
 * Converts a scancode to a character. This includes non-printable characters like tab, return, and backspace.
 */
static inline char _kbdev_get_ch(uint16_t scan, uint8_t mods) {
    if (mods & ~(KBD_MOD_SHIFT | KBD_MOD_CAPSLOCK)) {
        if (mods == KBD_MOD_CTRL) {
            return map_ctrl[scan];
        }
        // Other modifiers pressed; ignore
        return 0;
    } else if (mods & KBD_MOD_SHIFT) {
        if (mods & KBD_MOD_CAPSLOCK) {
            // Both shift and caps lock
            if (scan >= KEY_A && scan <= KEY_Z) {
                return map_lowercase[scan];
            } else {
                return map_uppercase[scan];
            }
        } else {
            // Shift only
            return map_uppercase[scan];
        }
    } else if (mods & KBD_MOD_CAPSLOCK) {
        // Caps lock only
        if (scan >= KEY_A && scan <= KEY_Z) {
            return map_uppercase[scan];
        } else {
            return map_lowercase[scan];
        }
    } else {
        return map_lowercase[scan];
    }
}

/**
 * Convert a scancode to a printable character, taking into account shift and caps lock states.
 */
static inline char _kbdev_get_printable(uint16_t scan, uint8_t mods) {
    if (!_kbdev_is_printable(scan)) {
        return 0;
    }
    return _kbdev_get_ch(scan, mods);
}