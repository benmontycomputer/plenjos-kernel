#include "devices/input/keyboard/ps2kbd.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/irq.h"
#include "devices/input/keyboard/keyboard.h"
#include "devices/io/ports.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "plenjos/dev/kbd.h"

#include <stdbool.h>
#include <stdint.h>

bool in_console_mode = true;

int get_scan_code() {
    return inb(PS2_DATA);
}

// This could be more memory efficient if stored as a bitmap
uint8_t is_key_pressed[32] = { 0 };

bool caps = false;

static struct {
    bool e0;          /* extended prefix */
    bool f0;          /* pause prefix */
    bool e1;          /* pause sequence in progress */
    uint8_t e1_count; /* bytes consumed in pause */
} ps2_kbd_state = { 0 };

static uint16_t ps2_lookup(uint8_t scancode) {
    return ps2_kbd_state.e0 ? _PS2_EXTENDED_LOOKUP[scancode] : _PS2_NORMAL_LOOKUP[scancode];
}

static uint8_t get_mods() {
    uint8_t mods = 0;

    if (is_key_pressed[KEY_LEFT_SHIFT / 8] & (1 << (KEY_LEFT_SHIFT % 8))) {
        mods |= KBD_MOD_SHIFT;
    }
    if (is_key_pressed[KEY_RIGHT_SHIFT / 8] & (1 << (KEY_RIGHT_SHIFT % 8))) {
        mods |= KBD_MOD_SHIFT;
    }
    if (caps) {
        mods |= KBD_MOD_CAPSLOCK;
    }
    // ctrl, alt, meta
    if (is_key_pressed[KEY_LEFT_CTRL / 8] & (1 << (KEY_LEFT_CTRL % 8))) {
        mods |= KBD_MOD_CTRL;
    }
    if (is_key_pressed[KEY_RIGHT_CTRL / 8] & (1 << (KEY_RIGHT_CTRL % 8))) {
        mods |= KBD_MOD_CTRL;
    }
    if (is_key_pressed[KEY_LEFT_ALT / 8] & (1 << (KEY_LEFT_ALT % 8))) {
        mods |= KBD_MOD_ALT;
    }
    if (is_key_pressed[KEY_RIGHT_ALT / 8] & (1 << (KEY_RIGHT_ALT % 8))) {
        mods |= KBD_MOD_ALT;
    }
    if (is_key_pressed[KEY_LEFT_META / 8] & (1 << (KEY_LEFT_META % 8))) {
        mods |= KBD_MOD_META;
    }
    if (is_key_pressed[KEY_RIGHT_META / 8] & (1 << (KEY_RIGHT_META % 8))) {
        mods |= KBD_MOD_META;
    }

    return mods;
}

void keyboard_irq_routine(registers_t *regs) {
    uint8_t in = inb(PS2_DATA);
    // printf("kbd: scancode %x\n", in);

    if (ps2_kbd_state.e1) {
        if (in != PS2_PAUSE_SEQ[ps2_kbd_state.e1_count++]) {
            // Invalid pause sequence; reset state
            printf("I/O ERROR: invalid pause sequence byte: expected %x, got %x\n",
                   PS2_PAUSE_SEQ[ps2_kbd_state.e1_count - 1], in);
            ps2_kbd_state.e1       = false;
            ps2_kbd_state.e1_count = 0;
            goto end;
        }

        if (ps2_kbd_state.e1_count == sizeof(PS2_PAUSE_SEQ)) {
            // Completed pause sequence
            kbd_event_t event;
            event.code  = KEY_PAUSE;
            event.state = KEY_PRESSED;
            event.mods  = get_mods();

            if (in_console_mode) {
                kbd_buffer_push(event);
            }

            // Reset state
            ps2_kbd_state.e1       = false;
            ps2_kbd_state.e1_count = 0;

            goto end;
        }
    }

    switch (in) {
    case PS2_EXTENDED_SCANCODE_PREFIX:
        ps2_kbd_state.e0 = true;
        goto end;
    case PS2_RELEASE_PREFIX:
        ps2_kbd_state.f0 = true;
        goto end;
    case PS2_PAUSE_PREFIX:
        ps2_kbd_state.e1       = true;
        ps2_kbd_state.e1_count = 1; // Already consumed first byte
        goto end;
    default:
        break;
    }

    // Normal scancode
    uint16_t keycode = ps2_lookup(in & 0x7F);
    bool keystate    = ps2_kbd_state.f0 ? false : true;
    if (in & 0x80) {
        // Should not happen; ignore
        printf("WARNING: received high bit set scancode %x\n", in);
        keystate = false;
    }

    ps2_kbd_state.e0 = false;
    ps2_kbd_state.f0 = false;

    kbd_event_t event;
    event.code = keycode;

    if (keycode < 0xFF) {
        event.state = keystate ? ((is_key_pressed[keycode / 8] & (1 << (keycode % 8))) ? KEY_REPEAT : KEY_PRESSED)
                               : KEY_RELEASED;

        // Update is_key_pressed array
        if (keystate) {
            is_key_pressed[keycode / 8] |= (1 << (keycode % 8));
        } else {
            is_key_pressed[keycode / 8] &= ~(1 << (keycode % 8));
        }
    } else {
        printf("WARNING: not ready for keycode %x >= 0xFF\n", keycode);
        event.state = keystate ? KEY_PRESSED : KEY_RELEASED;
    }

    if (event.state == KEY_PRESSED && (keycode == KEY_CAPS_LOCK)) {
        caps = !caps;
    }

    event.mods = get_mods();

    /* printf("kbd: scancode %x keystate %s -> keycode %x (%s), mods %b\n", in, keystate ? "PRESSED" : "RELEASED", keycode,
           (event.state == KEY_PRESSED)    ? "PRESSED"
           : (event.state == KEY_RELEASED) ? "RELEASED"
                                           : "REPEAT",
           event.mods); */

    if (in_console_mode) {
        kbd_buffer_push(event);
    }

end:
    // printf("kbd: sending EOI\n");
    apic_send_eoi();
}

static void ps2_wait_before_writing() {
    while (inb(PS2_STATUS) & PS2_IBF) {
        io_wait();
    }
}

static void ps2_wait_before_reading() {
    while (!(inb(PS2_STATUS) & PS2_OBF)) {
        io_wait();
    }
}

static uint8_t ps2_read_data() {
    ps2_wait_before_reading();
    return inb(PS2_DATA);
}

static void ps2_write_cmd(uint8_t cmd) {
    ps2_wait_before_writing();
    outb(PS2_COMMAND, cmd);
}

static void ps2_write_data(uint8_t data) {
    ps2_wait_before_writing();
    outb(PS2_DATA, data);
}

static void ps2_flush() {
    while (inb(PS2_STATUS) & PS2_OBF) {
        inb(PS2_DATA);
    }
}

static int ps2_reset_device(int port) {
    if (port == 2) {
        ps2_write_cmd(PS2_WRITE_PORT2);
    }

    ps2_write_data(PS2_DEV_RESET);

    uint8_t resp = ps2_read_data();

    if (resp != PS2_ACK) {
        printf("PS/2 device on port %d did not ACK reset command! got %x\n", port, resp);
        return -1;
    }

    resp = ps2_read_data();

    if (resp != PS2_SELFTEST_OK) {
        printf("PS/2 device on port %d failed self-test! got %x\n", port, resp);
        return -1;
    }

    return 0;
}

void register_keyboard_irq() {
    irq_register_routine(KEYBOARD_IRQ, keyboard_irq_routine);
}

void unregister_keyboard_irq() {
    irq_unregister_routine(KEYBOARD_IRQ);
}

// TODO: make sure we are in set 2
// https://wiki.osdev.org/I8042_PS/2_Controller
int init_ps2_keyboard() {
    int res = 0;

    asm volatile("cli");

    register_keyboard_irq();

    apic_send_eoi();

    uint8_t config;

    /* 1. Disable both ports */
    ps2_write_cmd(PS2_DISABLE_PORT1);
    ps2_write_cmd(PS2_DISABLE_PORT2);

    /* 2. Flush buffers */
    ps2_flush();

    /* 3. Read config byte */
    ps2_write_cmd(PS2_READ_CONFIG);
    config = ps2_read_data();

    printf("PS/2 controller config initial status: %b\n", config);

    /* 4. Clear IRQs and translation */
    config &= ~(PS2_CFG_IRQ1_ENABLE | PS2_CFG_IRQ12_ENABLE); // Disable IRQs
    config &= ~PS2_CFG_TRANSLATION;                          // Disable translation

    ps2_write_cmd(PS2_WRITE_CONFIG);
    ps2_write_data(config);

    /* Debug: check actual config byte vs expected */
    ps2_write_cmd(PS2_READ_CONFIG);
    uint8_t cfg_act = ps2_read_data();

    printf("PS/2 controller config new status: %b; should be %b\n", cfg_act, config);

    ps2_wait_before_writing();

    /* 5. Controller self-test */
    ps2_write_cmd(PS2_SELF_TEST);
    if (ps2_read_data() != PS2_SELF_TEST_OK) {
        printf("PS/2 controller self-test failed!\n");
        res = -1;
        goto finally;
    }

    /* 6. Detect second port */
    ps2_write_cmd(PS2_ENABLE_PORT2);
    ps2_write_cmd(PS2_READ_CONFIG);
    config = ps2_read_data();

    bool has_port2 = (config & PS2_CFG_DISABLE_PORT2_CLK) == 0;

    if (!has_port2) {
        printf("PS/2 controller has only one port.\n");
    } else {
        printf("PS/2 controller has two ports.\n");
    }

    ps2_write_cmd(PS2_DISABLE_PORT2);

    /* 7. Test ports */
    ps2_write_cmd(PS2_TEST_PORT1);
    if (ps2_read_data() != 0x00) {
        printf("PS/2 port 1 test failed!\n");
        res = -1;
        goto finally;
    }

    if (has_port2) {
        ps2_write_cmd(PS2_TEST_PORT2);
        if (ps2_read_data() != 0x00) {
            printf("PS/2 port 2 test failed!\n");
            has_port2 = false;
        }
    }

    /* 8. Reset devices */
    if (ps2_reset_device(1) != 0) {
        printf("PS/2 port 1 device reset failed!\n");
        res = -1;
        goto finally;
    } else {
        printf("PS/2 port 1 device reset succeeded.\n");
    }

    if (has_port2) {
        if (ps2_reset_device(2) != 0) {
            printf("PS/2 port 2 device reset failed!\n");
            has_port2 = false;
        } else {
            printf("PS/2 port 2 device reset succeeded.\n");
        }
    }

    /* 9. Enable devices */
    ps2_write_cmd(PS2_ENABLE_PORT1);
    // TODO: enable this once we want mouse
    /* if (has_port2) {
        ps2_write_cmd(PS2_ENABLE_PORT2);
    } */

    /* I don't know exactly why this is necessary, but it has to be here. */
    ps2_flush();

    ps2_write_data(PS2_DEV_ENABLE);
    ps2_read_data(); // ACK

    // TODO: enable this once we want mouse
    /* if (has_port2) {
        ps2_write_cmd(PS2_WRITE_PORT2);
        ps2_write_data(PS2_DEV_ENABLE);
        ps2_read_data(); // ACK
    } */

    /* 10. Enable interrupts */
    config |= PS2_CFG_IRQ1_ENABLE;
    // TODO: enable this once we want mouse
    /* if (has_port2) {
        config |= PS2_CFG_IRQ12_ENABLE;
    } */

    ps2_write_cmd(PS2_WRITE_CONFIG);
    ps2_write_data(config);

    /* Debug: check actual config byte vs expected */
    ps2_write_cmd(PS2_READ_CONFIG);
    cfg_act = ps2_read_data();

    printf("PS/2 controller config new status: %b; should be %b\n", cfg_act, config);

    /* 11. Enable ports */
    ps2_write_cmd(PS2_ENABLE_PORT1);
    if (has_port2) {
        ps2_write_cmd(PS2_ENABLE_PORT2);
    }

finally:
    asm volatile("sti");
    return res;
}

const uint16_t _PS2_NORMAL_LOOKUP[256] = {
    [0x01] = KEY_F9,
    [0x03] = KEY_F5,
    [0x04] = KEY_F3,
    [0x05] = KEY_F1,
    [0x06] = KEY_F2,
    [0x07] = KEY_F12,
    [0x09] = KEY_F10,
    [0x0A] = KEY_F8,
    [0x0B] = KEY_F6,
    [0x0C] = KEY_F4,
    [0x0D] = KEY_TAB,
    [0x0E] = KEY_BACKTICK,

    [0x11] = KEY_LEFT_ALT,
    [0x12] = KEY_LEFT_SHIFT,
    [0x14] = KEY_LEFT_CTRL,
    [0x15] = KEY_Q,
    [0x16] = KEY_1,

    [0x1A] = KEY_Z,
    [0x1B] = KEY_S,
    [0x1C] = KEY_A,
    [0x1D] = KEY_W,
    [0x1E] = KEY_2,

    [0x21] = KEY_C,
    [0x22] = KEY_X,
    [0x23] = KEY_D,
    [0x24] = KEY_E,
    [0x25] = KEY_4,
    [0x26] = KEY_3,

    [0x29] = KEY_SPACE,
    [0x2A] = KEY_V,
    [0x2B] = KEY_F,
    [0x2C] = KEY_T,
    [0x2D] = KEY_R,
    [0x2E] = KEY_5,

    [0x31] = KEY_N,
    [0x32] = KEY_B,
    [0x33] = KEY_H,
    [0x34] = KEY_G,
    [0x35] = KEY_Y,
    [0x36] = KEY_6,

    [0x3A] = KEY_M,
    [0x3B] = KEY_J,
    [0x3C] = KEY_U,
    [0x3D] = KEY_7,
    [0x3E] = KEY_8,

    [0x41] = KEY_COMMA,
    [0x42] = KEY_K,
    [0x43] = KEY_I,
    [0x44] = KEY_O,
    [0x45] = KEY_0,
    [0x46] = KEY_9,

    [0x49] = KEY_PERIOD,
    [0x4A] = KEY_SLASH,
    [0x4B] = KEY_L,
    [0x4C] = KEY_SEMICOLON,
    [0x4D] = KEY_P,
    [0x4E] = KEY_MINUS,

    [0x52] = KEY_QUOTE,
    [0x54] = KEY_LBRACK,
    [0x55] = KEY_EQUALS,

    [0x58] = KEY_CAPS_LOCK,

    [0x59] = KEY_RIGHT_SHIFT,
    [0x5A] = KEY_ENTER,
    [0x5B] = KEY_RBRACK,
    [0x5D] = KEY_BACKSLASH,

    [0x66] = KEY_BACKSPACE,

    [0x69] = KEY_KP_1,
    [0x6B] = KEY_KP_4,
    [0x6C] = KEY_KP_7,

    [0x70] = KEY_KP_0,
    [0x71] = KEY_KP_PERIOD,
    [0x72] = KEY_KP_2,
    [0x73] = KEY_KP_5,
    [0x74] = KEY_KP_6,
    [0x75] = KEY_KP_8,
    [0x76] = KEY_ESC,
    [0x77] = KEY_NUM_LOCK,
    [0x78] = KEY_F11,
    [0x79] = KEY_KP_PLUS,
    [0x7A] = KEY_KP_3,
    [0x7B] = KEY_KP_MINUS,
    [0x7C] = KEY_KP_ASTERISK,
    [0x7D] = KEY_KP_9,
    [0x7E] = KEY_SCROLL_LOCK,
    [0x83] = KEY_F7,
};

const uint16_t _PS2_EXTENDED_LOOKUP[256] = {
    [0x11] = KEY_RIGHT_ALT,
    [0x14] = KEY_RIGHT_CTRL,

    [0x4A] = KEY_KP_SLASH,
    [0x5A] = KEY_ENTER,

    [0x69] = KEY_END,
    [0x6B] = KEY_LEFT,
    [0x6C] = KEY_HOME,
    [0x70] = KEY_INSERT,
    [0x71] = KEY_DELETE,
    [0x72] = KEY_DOWN,
    [0x74] = KEY_RIGHT,
    [0x75] = KEY_UP,
    [0x7A] = KEY_PAGE_DOWN,
    [0x7D] = KEY_PAGE_UP,
};

// Set 1
/* const uint16_t _PS2_NORMAL_LOOKUP[128] = {
    [0x01] = KEY_ESC,         [0x02] = KEY_1,
    [0x03] = KEY_2,           [0x04] = KEY_3,
    [0x05] = KEY_4,           [0x06] = KEY_5,
    [0x07] = KEY_6,           [0x08] = KEY_7,
    [0x09] = KEY_8,           [0x0A] = KEY_9,
    [0x0B] = KEY_0,           [0x0C] = KEY_MINUS,
    [0x0D] = KEY_EQUALS,      [0x0E] = KEY_BACKSPACE,
    [0x0F] = KEY_TAB,         [0x10] = KEY_Q,
    [0x11] = KEY_W,           [0x12] = KEY_E,
    [0x13] = KEY_R,           [0x14] = KEY_T,
    [0x15] = KEY_Y,           [0x16] = KEY_U,
    [0x17] = KEY_I,           [0x18] = KEY_O,
    [0x19] = KEY_P,           [0x1A] = KEY_LBRACK,
    [0x1B] = KEY_RBRACK,      [0x1C] = KEY_ENTER,
    [0x1D] = KEY_LEFT_CTRL,   [0x1E] = KEY_A,
    [0x1F] = KEY_S,           [0x20] = KEY_D,
    [0x21] = KEY_F,           [0x22] = KEY_G,
    [0x23] = KEY_H,           [0x24] = KEY_J,
    [0x25] = KEY_K,           [0x26] = KEY_L,
    [0x27] = KEY_SEMICOLON,   [0x28] = KEY_QUOTE,
    [0x29] = KEY_BACKTICK,    [0x2A] = KEY_LEFT_SHIFT,
    [0x2B] = KEY_BACKSLASH,   [0x2C] = KEY_Z,
    [0x2D] = KEY_X,           [0x2E] = KEY_C,
    [0x2F] = KEY_V,           [0x30] = KEY_B,
    [0x31] = KEY_N,           [0x32] = KEY_M,
    [0x33] = KEY_COMMA,       [0x34] = KEY_PERIOD,
    [0x35] = KEY_SLASH,       [0x36] = KEY_RIGHT_SHIFT,
    [0x37] = KEY_KP_ASTERISK, [0x38] = KEY_LEFT_ALT,
    [0x39] = KEY_SPACE,       [0x3A] = KEY_CAPS_LOCK,
    [0x3B] = KEY_F1,          [0x3C] = KEY_F2,
    [0x3D] = KEY_F3,          [0x3E] = KEY_F4,
    [0x3F] = KEY_F5,          [0x40] = KEY_F6,
    [0x41] = KEY_F7,          [0x42] = KEY_F8,
    [0x43] = KEY_F9,          [0x44] = KEY_F10,
    [0x45] = KEY_NUM_LOCK,    [0x46] = KEY_SCROLL_LOCK,
    [0x47] = KEY_HOME,        [0x48] = KEY_UP,
    [0x49] = KEY_PAGE_UP,     [0x4B] = KEY_LEFT,
    [0x4C] = KEY_KP_CENTER,   [0x4D] = KEY_RIGHT,
    [0x4F] = KEY_END,         [0x50] = KEY_DOWN,
    [0x51] = KEY_PAGE_DOWN,   [0x52] = KEY_INSERT,
    [0x53] = KEY_DELETE,      [0x57] = KEY_F11,
    [0x58] = KEY_F12,         [0x77] = KEY_PAUSE, /* only part of the multi-byte pause sequence *//*
}; */

/* const uint16_t _PS2_EXTENDED_LOOKUP[128] = {
    [0x1C] = KEY_KP_ENTER, [0x1D] = KEY_RIGHT_CTRL, [0x35] = KEY_KP_SLASH, [0x38] = KEY_RIGHT_ALT, [0x47] = KEY_HOME,
    [0x48] = KEY_UP,       [0x49] = KEY_PAGE_UP,    [0x4B] = KEY_LEFT,     [0x4D] = KEY_RIGHT,     [0x4F] = KEY_END,
    [0x50] = KEY_DOWN,     [0x51] = KEY_PAGE_DOWN,  [0x52] = KEY_INSERT,   [0x53] = KEY_DELETE,
}; */

const uint8_t PS2_PAUSE_SEQ[8] = PS2_PAUSE_SEQ_DEF;