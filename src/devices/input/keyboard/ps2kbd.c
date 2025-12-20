#include "devices/input/keyboard/ps2kbd.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/irq.h"
#include "devices/input/keyboard/keyboard.h"
#include "devices/io/ports.h"
#include "kernel.h"
#include "lib/stdio.h"

#include <stdbool.h>
#include <stdint.h>

const uint32_t lowercase[128]
    = { UNKNOWN, ESC,     '1',     '2',       '3',         '4',     '5',     '6',     '7',     '8',     '9',
        '0',     '-',     '=',     '\b',      '\t',        'q',     'w',     'e',     'r',     't',     'y',
        'u',     'i',     'o',     'p',       '[',         ']',     '\n',    CTRL,    'a',     's',     'd',
        'f',     'g',     'h',     'j',       'k',         'l',     ';',     '\'',    '`',     LSHIFT,  '\\',
        'z',     'x',     'c',     'v',       'b',         'n',     'm',     ',',     '.',     '/',     RSHIFT,
        '*',     ALT,     ' ',     CAPS_LOCK, F1,          F2,      F3,      F4,      F5,      F6,      F7,
        F8,      F9,      F10,     NUM_LOCK,  SCROLL_LOCK, HOME,    UP,      PAGE_UP, '-',     LEFT,    UNKNOWN,
        RIGHT,   '+',     END,     DOWN,      PAGE_DOWN,   INSERT,  DELETE,  UNKNOWN, UNKNOWN, UNKNOWN, F11,
        F12,     UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
        UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
        UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
        UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN };
const uint32_t uppercase[128]
    = { UNKNOWN, ESC,     '!',     '@',       '#',         '$',     '%',     '^',     '&',     '*',     '(',
        ')',     '_',     '+',     '\b',      '\t',        'Q',     'W',     'E',     'R',     'T',     'Y',
        'U',     'I',     'O',     'P',       '{',         '}',     '\n',    CTRL,    'A',     'S',     'D',
        'F',     'G',     'H',     'J',       'K',         'L',     ':',     '"',     '~',     LSHIFT,  '|',
        'Z',     'X',     'C',     'V',       'B',         'N',     'M',     '<',     '>',     '?',     RSHIFT,
        '*',     ALT,     ' ',     CAPS_LOCK, F1,          F2,      F3,      F4,      F5,      F6,      F7,
        F8,      F9,      F10,     NUM_LOCK,  SCROLL_LOCK, HOME,    UP,      PAGE_UP, '-',     LEFT,    UNKNOWN,
        RIGHT,   '+',     END,     DOWN,      PAGE_DOWN,   INSERT,  DELETE,  UNKNOWN, UNKNOWN, UNKNOWN, F11,
        F12,     UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
        UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
        UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
        UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,   UNKNOWN,     UNKNOWN, UNKNOWN };

bool in_console_mode = true;

// The shift key triggers both of these; caps lock only triggers one
// The caps lock key triggers both a press and release event the moment it's pressed; it doesn't trigger anything on
// release. Shift works like a normal key except it doesn't send repeated press events
bool shift = false;
bool caps  = false;

char process_ch(char ch) {
    if (ch >= NUM_1 && ch <= NUM_0) {
        // Number key; only affected by shift (not caps)
        return shift ? uppercase[ch] : lowercase[ch];
    } else {
        // Affected by caps (which takes shift into effect)
        return caps ? uppercase[ch] : lowercase[ch];
    }
}

void process_mod(char ch, bool press) {
    switch (ch) {
    case LSHIFT: {
        shift = press;
        caps  = !caps;
        break;
    }
    case RSHIFT: {
        shift = press;
        caps  = !caps;
        break;
    }
    case CAPS_LOCK: {
        if (press) {
            caps = !caps;
        }
        break;
    }
    default: {
        break;
    }
    }
}

int get_scan_code() {
    return inb(PS2_ADDR_RW_DATA);
}

void keyboard_irq_routine(registers_t *regs) {
    char scancode = inb(PS2_ADDR_RW_DATA) & 0x7F;
    bool keystate = !((inb(PS2_ADDR_RW_DATA) & 0x80) == 0x80);

    // printf("Key %d %s.\n", scancode, keystate ? "pressed" : "released");

    process_mod(scancode, keystate);

    if (keystate && in_console_mode) {
        char ch = process_ch(scancode);
        if (!(ch == scancode) && ch != (char)-1) kbd_buffer_push(ch);
        else if (ch == (char)UP || ch == (char)DOWN || ch == (char)LEFT || ch == (char)RIGHT) {
            kbd_buffer_push((char)-1);
            kbd_buffer_push(ch);
        }
        // printf("Key %c.\n", ch);
    }

    apic_send_eoi();
}

void register_keyboard_irq() {
    irq_register_routine(KEYBOARD_IRQ, keyboard_irq_routine);
}

void unregister_keyboard_irq() {
    irq_unregister_routine(KEYBOARD_IRQ);
}

void ps2_wait_before_writing() {
    while (inb(0x64) & 0b10) {
        io_wait();
    }
}

void ps2_wait_before_reading() {
    while (!(inb(0x64) & 0b1)) {
        io_wait();
    }
}

// https://wiki.osdev.org/I8042_PS/2_Controller
void init_ps2_keyboard() {
    asm volatile("cli");

    register_keyboard_irq();

    apic_send_eoi();

    while (inb(0x64) & 0b10) {
        printf("char\n");
        io_wait();
    }

    ps2_wait_before_writing();

    // Disable device 1
    outb(0x64, 0xAD);

    ps2_wait_before_writing();

    // Disable device 2 (if present)
    outb(0x64, 0xA7);

    while (inb(0x60) && (inb(0x64) & 0b1)) {
        io_wait();
    }

    ps2_wait_before_writing();

    outb(0x64, 0x20);

    ps2_wait_before_reading();

    uint8_t b = inb(0x60);

    printf("PS/2 controller config previous status: %b\n", b);

    b &= ~0b01000001; // clear bits 0 and 6 to disable IRQs for port 1

    ps2_wait_before_writing();

    outb(0x64, 0x60);

    ps2_wait_before_writing();

    outb(0x64, b);

    printf("PS/2 controller config new status: %b\n", b);

    ps2_wait_before_writing();

    outb(0x64, 0xAA);

    ps2_wait_before_reading();

    // self-test the PS/2 controller
    uint8_t c = inb(0x60);

    printf("PS/2 controller test result: %x (%s)\n", c, (c == 0x55) ? "pass" : "fail");

    bool port_one = false;
    bool port_two = false;

    if (c == 0x55) {
        ps2_wait_before_writing();

        // Try to enable the second channel
        outb(0x64, 0xA8);

        ps2_wait_before_writing();

        outb(0x64, 0x20);

        ps2_wait_before_reading();

        uint8_t new_status = inb(0x60);

        if (new_status & 0b100000) {
            // No second channel
            port_one = true;
        } else {
            // Second channel
            port_one = true;
            port_two = true;
            printf("PS/2 controller has two channels.\n");
        }

        // Test the first PS/2 port
        ps2_wait_before_writing();

        outb(0x64, 0xAB);

        ps2_wait_before_reading();

        uint8_t port_test_result = inb(0x60);

        if (port_test_result != 0) {
            printf("PS/2 port 1 test failed! code: %x\n", port_test_result);

            port_one = false;
        }

        if (port_two) {
            // Test the second PS/2 port
            ps2_wait_before_writing();

            outb(0x64, 0xA9);

            ps2_wait_before_reading();

            uint8_t port_test_result = inb(0x60);

            if (port_test_result != 0) {
                printf("PS/2 port 2 test failed! code: %x\n", port_test_result);

                port_two = false;
            }
        }

        printf("PS/2 controller config status: %b\n", b);
    }

    if (port_one) {
        // Enable the first PS/2 port
        ps2_wait_before_writing();
        outb(0x64, 0xAE);
        printf("Enabled PS/2 port 1.\n");

        // Enable IRQs for the first PS/2 port
        b |= 0b1;
    }

    if (port_two) {
        // Enable the second PS/2 port
        ps2_wait_before_writing();
        outb(0x64, 0xAE);
        printf("Enabled PS/2 port 2.\n");

        // Enable IRQs for the second PS/2 port
        b |= 0b10;
    }

    ps2_wait_before_writing();

    outb(0x64, 0x60);

    ps2_wait_before_writing();

    outb(0x64, b);

    printf("PS/2 controller final config status: %b\n", b);

    // TODO: reset devices on both ports

    /* while (inb(0x64) & 0x02) {
        printf("char\n");
        io_wait();
    } */

    asm volatile("sti");
}