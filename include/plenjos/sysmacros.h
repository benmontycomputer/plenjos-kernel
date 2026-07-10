#pragma once

#include "plenjos/types.h"
#include "plenjos/dirent.h"
#include "plenjos/stat.h"

#define _major(d) (((d) & 0xFFFFFF0000000000) >> 40)
#define _minor(d) ((d) & 0x000000FFFFFFFFFF)

#define full_dev(maj, min) ((((maj) & 0xFFFFFF) << 40) | ((min) & 0xFFFFFFFFFF))

inline dirent_type_t mode_to_dt(mode_t mode) {
    switch (mode & S_IFMT) {
    case S_IFREG:
        return DT_REG;
    case S_IFDIR:
        return DT_DIR;
    case S_IFLNK:
        return DT_LNK;
    case S_IFCHR:
        return DT_CHR;
    case S_IFBLK:
        return DT_BLK;
    case S_IFIFO:
        return DT_FIFO;
    case S_IFSOCK:
        return DT_SOCK;
    default:
        return DT_UNKNOWN;
    }
}