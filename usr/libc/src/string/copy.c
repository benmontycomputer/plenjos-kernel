#include "string.h"

#include <stdint.h>

/* Copying functions */

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

char *strcpy(char *dest, const char *src) {
    size_t len = strlen(src) + 1;
    memcpy(dest, src, len);
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    if (n == 0) return dest;

    size_t len = strlen(src);

    if (len >= n) {
        memcpy(dest, src, n);
    } else {
        memcpy(dest, src, len);
        while (len < n) {
            dest[len] = 0;
            len++;
        }
    }

    return dest;
}