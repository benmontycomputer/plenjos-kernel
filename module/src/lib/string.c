#include <stdint.h>

#include "string.h"
#include "common.h"

// Placeholder until I make the real thing; https://stackoverflow.com/questions/32560167/strncmp-implementation
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) {
        return 0;
    } else {
        return (*(unsigned char *)s1 - *(unsigned char *)s2);
    }
}

// TODO: is this the fastest implementation?
size_t strlen(const char *s) {
    const char *adj = s;

    while (*adj) {
        ++adj;
    }

    return (size_t)(adj - s);
}

char *strncpy(char *dest, const char *src, size_t n) {
    if (strlen(src) >= n) {
        memcpy(dest, src, n - 1);
        dest[n - 1] = 0;
    } else {
        memcpy(dest, src, n);
    }
}