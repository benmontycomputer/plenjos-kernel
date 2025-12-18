#ifndef _PLENJOS_STAT_H
#define _PLENJOS_STAT_H 1

#include "plenjos/types.h"

#include <stdint.h>

/* #define S_IFMT  0xF000 // File type mask
#define S_IFREG 0x8000 // Regular file
#define S_IFDIR 0x4000 // Directory */

#define S_IRUSR 00400 // Read permission, owner
#define S_IWUSR 00200 // Write permission, owner
#define S_IXUSR 00100 // Execute/search permission, owner

#define S_IRGRP 00040 // Read permission, group
#define S_IWGRP 00020 // Write permission, group
#define S_IXGRP 00010 // Execute/search permission, group

#define S_IROTH 00004 // Read permission, others
#define S_IWOTH 00002 // Write permission, others
#define S_IXOTH 00001 // Execute/search permission, others

#define S_ISUID 04000 // Set user ID on execution
#define S_ISGID 02000 // Set group ID on execution
#define S_ISVTX 01000 // Sticky bit

#define S_DFLT 0755 // Default mode for new files/directories: rwxr-xr-x

// To be filled out more later
struct stat {
    mode_t mode;
    uid_t uid;
    gid_t gid;
};

#endif /* _PLENJOS_STAT_H */