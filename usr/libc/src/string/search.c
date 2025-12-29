#include "string.h"

#include <stdint.h>

/* Search functions */

void *memchr(const void *s, int c, size_t n);
char *strchr(const char *s, int c);
size_t strcspn(const char *s, const char *accept);
char *strpbrk(const char *s, const char *accept);
char *strrchr(const char *s, int c);
size_t strspn(const char *s, const char *accept);
char *strstr(const char *haystack, const char *needle);

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