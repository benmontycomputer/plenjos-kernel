#ifndef _PLENJOS_STAT_H
#define _PLENJOS_STAT_H 1

#include "plenjos/posix_stat.h"

#include "plenjos/types.h"

#include <stdint.h>

// TODO: replace all instances of this with some combination of full perms and umask
#define S_DFLT 0755 // Default mode for new files/directories: rwxr-xr-x

// To be filled out more later
struct kstat {
    mode_t mode;
    uid_t uid;
    gid_t gid;

    off_t size;
};

#endif /* _PLENJOS_STAT_H */