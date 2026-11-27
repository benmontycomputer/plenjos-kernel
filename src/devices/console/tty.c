#include "tty.h"

#include "arch/x86_64/common.h"
#include "devices/manager.h"
#include "devices/sysdrivers.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "stddef.h"

static void tty_dump_unhandled_escape_sequence(struct tty *tty) {
    kout(KERNEL_INFO, "tty_default_write(): unhandled escape sequence 0x1b(ESC)");

    for (size_t i = 1; i < sizeof(tty->out_buf); i++) {
        if (!tty->out_buf[i]) {
            break;
        }
        kout(KERNEL_INFO, " %.2x(%c)", (uint32_t)tty->out_buf[i], tty->out_buf[i]);
    }
    kout(KERNEL_INFO, "\n");
}

static int tty_default_out(void *func_args) {
    struct tty *tty = func_args;
#ifdef _KERNEL_EXTRA_CHECKS_FOR_NULL_ARGS
    if (!tty) {
        kout(KERNEL_PANIC, "tty_default_out(): tty is NULL!");
    }
#endif

    unsigned i = 1;
    int res    = 0;
    char ch    = tty->out_buf[0];
    switch (ch) {
    case '\x1b':
        while (i < sizeof(tty->out_buf) && tty->out_buf[i]) {
            ch = tty->out_buf[i];
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
                break;
            }
            i++;
        }
#ifdef _KERNEL_TTY_BUFFER_CORRECTNESS_CHECKS
        if (i == sizeof(tty->out_buf)) {
            kout(KERNEL_PANIC, "tty_default_write(): can't find end of sequence!\n");
            res = -EIO;
        } else if (!tty->out_buf[i++]) {
            kout(KERNEL_PANIC,
                 "tty_default_write(): incomplete escape sequence but out_buf wasn't full (shouldn't happen)\n");
        } else {
#endif
            if (i == 4 && !memcmp(tty->out_buf, "\x1b[2J", i)) {
                // Clear
                clear();
            } else if (i == 6 && !memcmp(tty->out_buf, "\x1b[?12h", i)) {
                // Enable blinking
                // TODO: blinking
                setcursor(true);
            } else if (i == 6 && !memcmp(tty->out_buf, "\x1b[?12l", i)) {
                // Disable blinking
                // TODO: blinking
                setcursor(false);
            } else if (i == 6 && !memcmp(tty->out_buf, "\x1b[?25h", i)) {
                // Enable cursor
                setcursor(true);
            } else if (i == 6 && !memcmp(tty->out_buf, "\x1b[?25l", i)) {
                // Disable cursor
                setcursor(false);
            } else if ((i == 3 && !memcmp(tty->out_buf, "\x1b[K", i)) || (i == 4 && !memcmp(tty->out_buf, "\x1b[0K", i))) {
                // Clear from cursor (inclusive) to end of line
                console_clear_to_eol();
            } else if (i == 4 && !memcmp(tty->out_buf, "\x1b[7m", i)) {
                // Invert
                set_invert(true);
            } else if ((i == 3 && !memcmp(tty->out_buf, "\x1b[m", i)) || (i == 4 && !memcmp(tty->out_buf, "\x1b[0m", i))) {
                // Reset
                set_invert(false);
                set_color(DEFAULT_FG, DEFAULT_BG);
            } else if (tty->out_buf[i - 1] == 'H' || tty->out_buf[i - 1] == 'f') {
                // Move cursor
                if (i == 3) {
                    if (tty->out_buf[i - 1] == 'H') {
                        console_seek_fb(0, 0);
                    }
                } else if (i > 2 && tty->out_buf[1] == '[') {
                    uint64_t row = 0;
                    uint64_t col = 0;

                    bool on_col = false;

                    for (unsigned j = 2; j < i - 1; j++) {
                        char outch = tty->out_buf[j];
                        if (outch == ';') {
                            if (!on_col) {
                                on_col = true;
                                continue;
                            }
                        } else if (outch >= '0' && outch <= '9') {
                            if (on_col) {
                                col *= 10;
                                col += outch - '0';
                            } else {
                                row *= 10;
                                row += outch - '0';
                            }
                            continue;
                        }

                        on_col = false;
                        break;
                    }

                    // TODO: check x and y's max bounds
                    if (on_col && row > 0 && col > 0) {
                        console_seek_fb(col - 1, row - 1);
                    } else {
                        tty_dump_unhandled_escape_sequence(tty);
                    }
                } else {
                    tty_dump_unhandled_escape_sequence(tty);
                }
            } else {
                tty_dump_unhandled_escape_sequence(tty);
            }
#ifdef _KERNEL_TTY_BUFFER_CORRECTNESS_CHECKS
        }
#endif
        break;
    case '\b':
        backs();
        break;
    default:
        printf_ch_fb(ch);
        break;
    }
#ifdef _KERNEL_TTY_BUFFER_CORRECTNESS_CHECKS
    if (i <= sizeof(tty->out_buf) && tty->out_buf[i]) {
        kout(KERNEL_PANIC, "tty_default_write(): excess char(s) in out buf\n");
    }
#endif
    memset(tty->out_buf, 0, sizeof(tty->out_buf));
    return res;
}

struct tty tty_main = {
    .in  = NULL,
    .out = tty_default_out,
};

ssize_t tty_read(vfs_handle_t *handle, void *buf, size_t len);
ssize_t tty_write(vfs_handle_t *handle, const void *buf, size_t len);

endpoint_driver_t _tty_endpoint_driver = {
    .flags = 0,
    .major = ENDPOINT_TERM,
    .read  = tty_read,
    .write = tty_write,
    .seek  = NULL,
};

int setup_main_tty() {
    endpoint_t *endpoint = register_device_endpoint("tty0", &_tty_endpoint_driver, 0, NULL, &tty_main);
    return endpoint ? 0 : -EIO;
}

inline size_t tty_flush_in_buf(void *buf, size_t len, struct tty *tty) {
    size_t i = 0;
    while (i < len && i < sizeof(tty->in_buf) && tty->in_buf[i]) {
        *((char *)buf + i) = tty->in_buf[i];
        i++;
    }

    for (size_t j = 0; j < sizeof(tty->in_buf); j++) {
        tty->in_buf[j] = (j + i < sizeof(tty->in_buf)) ? tty->in_buf[j + i] : 0;
    }
    return i;
}

ssize_t tty_read(vfs_handle_t *handle, void *buf, size_t len) {
    bool interrupts = are_interrupts_enabled();
    if (interrupts) {
        asm volatile("cli");
    }

    endpoint_t *endpoint = devfs_get_endpoint_from_handle(handle);
    struct tty *tty      = endpoint->func_args;

#ifdef _KERNEL_EXTRA_CHECKS_FOR_NULL_ARGS
    if (!tty) {
        kout(KERNEL_PANIC, "tty_read(): tty is NULL!\n");
    }
#endif
    size_t i = 0;

    if (!tty->in) {
        kout(KERNEL_WARN, "tty_read(): tty->in is NULL!\n");
        i = -EIO;
        goto ret;
    }

    mutex_lock(&tty->in_buf_lock);

    while (i < len) {
        i += tty_flush_in_buf((uint8_t *)buf + i, len - i, tty);

        if (i >= len) {
            break;
        }

        int res = tty->in(tty);

        if (res) {
            break;
        }
    }

    mutex_unlock(&tty->in_buf_lock);

ret:
    if (interrupts) {
        asm volatile("sti");
    }

    return i;
}

ssize_t tty_write(vfs_handle_t *handle, const void *buf, size_t len) {
    endpoint_t *endpoint = devfs_get_endpoint_from_handle(handle);
    struct tty *tty      = endpoint->func_args;

#ifdef _KERNEL_EXTRA_CHECKS_FOR_NULL_ARGS
    if (!tty) {
        kout(KERNEL_PANIC, "tty_write(): tty is NULL!\n");
    }
#endif

    if (!tty->out) {
        kout(KERNEL_WARN, "tty_write(): tty->out is NULL!\n");
        return -EIO;
    }

    size_t i = 0;

    mutex_lock(&tty->out_buf_lock);

    while (i < len) {
        char ch = ((char *)buf)[i];
        if (ch == '\x1b' || tty->out_buf[0]) {
            size_t j = 0;
            while (tty->out_buf[j] && j < sizeof(tty->out_buf)) {
                j++;
            }
            while (i < len && j < sizeof(tty->out_buf)) {
                ch              = ((char *)buf)[i];
                tty->out_buf[j] = ch;
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
                    break;
                }
                i++;
                j++;
            }

            if (j >= sizeof(tty->out_buf)) {
                kout(KERNEL_INFO, "tty_write(): incomplete escape sequence w/ full output buffer (not an error).\n");
                memset(tty->out_buf, 0, sizeof(tty->out_buf));
            } else if (i >= len) {
                break; // Sequence will finish later
            } else {
                tty->out(tty);
                i++;
            }
        } else {
            tty->out_buf[0] = ch;
            tty->out(tty);
            i++;
        }
    }

    mutex_unlock(&tty->out_buf_lock);
    return i;
}