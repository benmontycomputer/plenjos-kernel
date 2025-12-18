#ifndef _STDDEF_H
#define _STDDEF_H 1

#include "stdint.h"

typedef int64_t ptrdiff_t;
typedef uint64_t size_t;

// max_align_t
typedef struct {
    long long __ll;
    long double __ld;
} max_align_t;

typedef uint32_t wchar_t;

#define NULL ((void *)0)

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif /* _STDDEF_H */