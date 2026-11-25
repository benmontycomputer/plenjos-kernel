#include "stdlib.h"

int atoi(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n') s++; // Skip leading whitespace
    int sign = 1;
    if (s == '+') {
        s++;
    } else if (s == '-') {
        s++;
        sign = -1;
    }

    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (int)(*s++ - '0');
    }

    return sign * v;
}