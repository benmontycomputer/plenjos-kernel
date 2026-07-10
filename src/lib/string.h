#pragma once

#include <stddef.h>

inline char tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    } else {
        return c;
    }
}

int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
char *strncpy(char *dest, const char *src, size_t n);
char *strtok(char *str, const char *delim);

int strncmp_case_insensitive(const char *s1, const char *s2, size_t n);