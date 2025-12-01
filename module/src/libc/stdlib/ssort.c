#include "stdlib.h"

/* Searching and sorting utilities */

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    // By the C standard, if nmemb is 0, return NULL.
    // However, if base, key, or compar are NULL or if nmemb is 0, behavior is undefined, so we don't need to check
    // those.
    if (nmemb == 0) { return NULL; }

    size_t left = 0;
    // This is always exclusive
    size_t right = nmemb;
    size_t mid = 0;

    while (left < right) {
        mid = left + (right - left) / 2;

        int cmp = compar(key, (char *)base + (mid * size));

        if (cmp < 0) {
            // Need to search [left, mid)
            right = mid;
        } else if (cmp > 0) {
            // Need to search (mid, right)
            left = mid + 1;
        } else {
            // Match!
            return (char *)base + (mid * size);
        }
    }

    return NULL;
}

// TODO: can we optomize this using SIMD or larger data types? (similar to memset or memcpy or memmove or something)
static void _swap(size_t size, char *s1, char *s2) {
    char tmp;
    for (size_t i = 0; i < size; i++) {
        tmp = s1[i];
        s1[i] = s2[i];
        s2[i] = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    // TODO: use quicksort or another algorithm
    // Currently, we use insertion sort.
    for (size_t i = size; i < nmemb * size; i += size)
        for (size_t j = i; j > 0 && compar((char *)base + j - size, (char *)base + j) > 0; j -= size)
            _swap(size, (char *)base + j - size, (char *)base + j);
}