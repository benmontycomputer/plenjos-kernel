#ifndef _PLENJOS_STAT_H
#define _PLENJOS_STAT_H 1

#include "plenjos/posix_stat.h"

#include "plenjos/types.h"

#include <stdint.h>

// TODO: replace all instances of this with some combination of full perms and umask
// TODO: IMPORTANT: files should default to 0644
#define S_DFLT 0755 // Default mode for new files/directories: rwxr-xr-x

// To be filled out more later
struct kstat {
    dev_t dev; // Device where this file is stored

    mode_t mode;
    uid_t uid;
    gid_t gid;

    dev_t rdev; // If block or character device, this is the dev_t for the endpoint

    off_t size;
};

#endif /* _PLENJOS_STAT_H */