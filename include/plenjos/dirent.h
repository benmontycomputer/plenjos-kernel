#ifndef _PLENJOS_DIRENT_H
#define _PLENJOS_DIRENT_H 1

#include <stdint.h>

#include "plenjos/dt.h"
#include "plenjos/limits.h"

// #define DT_MNT (DT_BLK | DT_DIR)

typedef unsigned char __kernel_dirent_type_t;
typedef __kernel_dirent_type_t dirent_type_t;

struct plenjos_dirent {
    char d_name[NAME_MAX + 1];
    dirent_type_t type;

    // uint8_t reserved[7];
} __attribute__((packed));

#endif /* _PLENJOS_DIRENT_H */