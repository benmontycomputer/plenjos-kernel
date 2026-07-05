#include "tty.h"

#include "devices/manager.h"
#include "devices/sysdrivers.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "stddef.h"

extern void printf_ch_fb(char ch);

int tty_default_out(void *func_args) {
    struct tty *tty = func_args;
    int i = 1;
    int res = 0;
    char ch = tty->out_buf[0];
    switch (ch) {
    case '\x1b':
        while (i < sizeof(tty->out_buf) && tty->out_buf[i]) {
            ch = tty->out_buf[i];
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
                break;
            }
            i++;
        }
        if (i == sizeof(tty->out_buf)) {
            kout(KERNEL_PANIC, "tty_default_write(): can't find end of sequence!\n");
            res = -EIO;
        } else if (!tty->out_buf[i++]) {
            kout(KERNEL_PANIC, "tty_default_write(): incomplete escape sequence but out_buf wasn't full (shouldn't happen)\n");
        } else {
            if (i == 4 && !memcmp(tty->out_buf, "\x1b[2J", i)) {
                // Clear
                clear();
            } else if (i == 6 && !memcmp(tty->out_buf, "\x1b[?12h", i)) {
                // Enable cursor
                setcursor(true);
            } else if (i == 6 && !memcmp(tty->out_buf, "\x1b[?12l", i)) {
                // Disable cursor
                setcursor(false);
            } else {
                kout(KERNEL_INFO, "tty_default_write(): unhandled escape sequence");

                for (int i = 0; i < sizeof(tty->out_buf); i++) {
                    if (!tty->out_buf[i]) {
                        break;
                    }
                    kout(KERNEL_INFO, " %.2x(%c)", (uint32_t)tty->out_buf[i], tty->out_buf[i]);
                }
                kout(KERNEL_INFO, "\n");
            }
        }
        break;
    case '\b':
        backs();
        break;
    default:
        printf_ch_fb(ch);
        break;
    }
    if (i <= sizeof(tty->out_buf) && tty->out_buf[i]) {
        kout(KERNEL_WARN, "tty_default_write(): ignoring excess char(s) in out buf\n");
    }
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
    endpoint_t *endpoint = devfs_get_endpoint_from_handle(handle);
    struct tty *tty      = endpoint->func_args;

    if (!tty || !tty->in) {
        kout(KERNEL_WARN, "tty_read(): tty or tty->in is NULL!\n");
        return -EIO;
    }

    size_t i = 0;

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
    return i;
}

ssize_t tty_write(vfs_handle_t *handle, const void *buf, size_t len) {
    endpoint_t *endpoint = devfs_get_endpoint_from_handle(handle);
    struct tty *tty      = endpoint->func_args;

    if (!tty || !tty->out) {
        kout(KERNEL_WARN, "tty_write(): tty or tty->out is NULL!\n");
        return -EIO;
    }

    size_t i = 0;

    while (i < len) {
        char ch = ((char *)buf)[i];
        if (ch == '\x1b' || tty->out_buf[0]) {
            size_t j = 0;
            while (tty->out_buf[j] && j < sizeof(tty->out_buf)) {
                j++;
            }
            while (i < len && j < sizeof(tty->out_buf)) {
                ch = ((char *)buf)[i];
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
    return i;
}