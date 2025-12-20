#include <stdint.h>

#include "lib/string.h"

#include "kernel.h"

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

    return dest;
}

static char *olds;

// Written by Plenjamin
char *strtok(char *str, const char *delim) {
    if (str == NULL) str = olds;
    if (str == NULL) return NULL;
    if (delim == NULL) return NULL;

    char *d = NULL;

    // First, skip over any leading delimiters
    while (*str) {
        d = (char *)delim;

        while (*d) {
            if (*str == *d) break;
            d++;
        }

        if (*d == '\0') break;

        ++str;
    }

    if (*str == '\0') {
        olds = NULL;
        return NULL;
    }

    char *s = str;

    while (*str) {
        d = (char *)delim;

        while (*d) {
            if (*str == *d) {
                *str = '\0';
                olds = ++str;
                return s;
            }
            d++;
        }

        ++str;
    }

    olds = NULL;
    return s;
}