#include "keyboard.h"

#include "devices/console/tty.h"
#include "devices/manager.h"
#include "devices/sysdrivers.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "lib/string.h"

#include <stdbool.h>
#include <stdint.h>

static fixed_size_allocator_t _kbd_fsa = FSA_DEFAULT(sizeof(kbd_buffer_state_t));

#define MAX_KEYBOARDS 16

mutex keyboards_lock                         = MUTEX_INIT;
kbd_buffer_state_t *keyboards[MAX_KEYBOARDS] = { NULL };

ssize_t kbd_read(vfs_handle_t *handle, void *buf, size_t len);

endpoint_driver_t _keyboard_endpoint_driver = {
    .flags = 0,
    .major = ENDPOINT_KBD,
    .read  = kbd_read,
    .write = NULL,
    .seek  = NULL,
};

static void advance_tail(kbd_buffer_state_t *kbd_buffer_state) {
    kbd_buffer_state->tail = (kbd_buffer_state->tail + 1) % KBD_BUFFER_SIZE;
}

static void advance_head(kbd_buffer_state_t *kbd_buffer_state) {
    kbd_buffer_state->head = (kbd_buffer_state->head + 1) % KBD_BUFFER_SIZE;
}

bool kbd_buffer_empty(kbd_buffer_state_t *kbd_buffer_state) {
    return (!kbd_buffer_state->full) && (kbd_buffer_state->head == kbd_buffer_state->tail);
}

bool kbd_buffer_full(kbd_buffer_state_t *kbd_buffer_state) {
    return kbd_buffer_state->full;
}

void kbd_buffer_push(kbd_buffer_state_t *kbd_buffer_state, kbd_event_t event) {
    kbd_buffer_state->buffer[kbd_buffer_state->head] = event;

    if (kbd_buffer_state->full) {
        advance_tail(kbd_buffer_state);
    }
    advance_head(kbd_buffer_state);

    kbd_buffer_state->full = (kbd_buffer_state->head == kbd_buffer_state->tail);
}

int kbd_buffer_pop(kbd_buffer_state_t *kbd_buffer_state, kbd_event_t *data) {
    if (kbd_buffer_empty(kbd_buffer_state)) return -1;

    *data = kbd_buffer_state->buffer[kbd_buffer_state->tail];

    advance_tail(kbd_buffer_state);

    kbd_buffer_state->full = false;

    return 0;
}

ssize_t kbd_read(vfs_handle_t *handle, void *buf, size_t len) {
    if (len % sizeof(kbd_event_t)) {
        kout(KERNEL_WARN, "kbd_read(): len must be a perfect multiple of sizeof(kbd_event_t)!");
        return -EINVAL;
    }

    endpoint_t *endpoint          = devfs_get_endpoint_from_handle(handle);
    kbd_buffer_state_t *buf_state = (kbd_buffer_state_t *)endpoint->func_args;
    ssize_t res                   = 0;
    while (res < len) {
        int pop_res = kbd_buffer_pop(buf_state, (kbd_event_t *)((uint8_t *)buf + res));
        if (pop_res) {
            break;
        }
        res += sizeof(kbd_event_t);
    }
    return res;
}

int kbd_console_in(void *func_args) {
    if (!func_args) {
        kout(KERNEL_PANIC, "kbd_console_in(): func_args was NULL!\n");
        return -EIO;
    }

    struct tty *tty = (struct tty *)func_args;
    if (tty->in_buf[0]) {
        kout(KERNEL_PANIC, "kbd_console_in(): tty->in_buf not cleared!\n");
    }

    kbd_event_t event = { 0 };
    int res;
run:
    res = kbd_buffer_pop(tty->in_data, &event);

    if (!res) {
        if (event.state != KEY_PRESSED) {
            goto run;
        }
        char ctmp = _kbdev_get_ch(event.code, event.mods);
        if (ctmp == 0) {
            switch (event.code) {
            case KEY_UP:
                strncpy(tty->in_buf, "\x1b[A", 3);
                break;
            case KEY_DOWN:
                strncpy(tty->in_buf, "\x1b[B", 3);
                break;
            case KEY_RIGHT:
                strncpy(tty->in_buf, "\x1b[C", 3);
                break;
            case KEY_LEFT:
                strncpy(tty->in_buf, "\x1b[D", 3);
                break;
            default:
                res = -1;
                break;
            }
        } else {
            tty->in_buf[0] = ctmp;
        }
    }

    return res;
}

int kbd_register(kbd_buffer_state_t **out, endpoint_t **endpoint_out) {
    if (!out) {
        kout(KERNEL_PANIC, "kbd_register(): out must not be NULL!\n");
        return -EINVAL;
    }
    mutex_lock(&keyboards_lock);
    for (int i = 0; i < MAX_KEYBOARDS; i++) {
        if (!keyboards[i]) {
            keyboards[i] = (kbd_buffer_state_t *)fsa_alloc(&_kbd_fsa);
            *out         = keyboards[i];

            char name[NAME_MAX + 1];
            populate_string_hexnum(name, sizeof(name), "kbd", (uint32_t)i);

            // TODO: hw dev
            endpoint_t *endpoint
                = register_device_endpoint(name, &_keyboard_endpoint_driver, (uint64_t)i, NULL, keyboards[i]);
            if (!endpoint) {
                kout(KERNEL_SEVERE_EXTERNAL_FAULT, "kbd_register(): register_device_endpoint() failed!\n");
                fsa_free(&_kbd_fsa, keyboards[i]);
                keyboards[i] = NULL;
                mutex_unlock(&keyboards_lock);
                return -EIO;
            }
            mutex_unlock(&keyboards_lock);
            if (endpoint_out) {
                *endpoint_out = endpoint;
            }

            tty_main.in_data = keyboards[i];
            tty_main.in      = kbd_console_in;

            return i;
        }
    }

    mutex_unlock(&keyboards_lock);
    kout(KERNEL_WARN, "kbd_register(): too many keyboards!\n");
    return -EIO;
}