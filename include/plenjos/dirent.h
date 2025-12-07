#ifndef _PLENJOS_DIRENT_H
#define _PLENJOS_DIRENT_H 1

#include <stdint.h>

#include "plenjos/limits.h"

#define DT_BLK 0x01
#define DT_CHR 0x02
#define DT_DIR 0x04
#define DT_FIFO 0x08
#define DT_LNK 0x10
#define DT_REG 0x20
#define DT_SOCK 0x40
#define DT_UNKNOWN 0x80
// #define DT_MNT (DT_BLK | DT_DIR)

typedef uint8_t __kernel_dirent_type_t;
typedef __kernel_dirent_type_t dirent_type_t;

struct plenjos_dirent {
    char d_name[NAME_MAX + 1];
    dirent_type_t type;

    uint8_t reserved[7];
} __attribute__((packed));

#endif /* _PLENJOS_DIRENT_H */