#include "stdio.h"
#include "sys/syscall.h"
#include "uconsole.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

FILE *stdin = NULL;

char getchar() {
    /* kbd_event_t event = { 0 };
    fread(&event, sizeof(event), 1, stdin);
    return _kbdev_get_ch(event.code, event.mods); */
    while (true) {
        char ch    = 0;
        size_t res = fread(&ch, 1, 1, stdin);
        if (res == 1) {
            return ch;
        } else {
            syscall_sleep(5);
        }
    }
}

/* void kbd_get(kbd_event_t *out) {
    fread(out, sizeof(*out), 1, stdin);
} */

char *gets(char *str) {}