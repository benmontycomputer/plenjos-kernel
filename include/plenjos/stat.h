#ifndef _PLENJOS_STAT_H
#define _PLENJOS_STAT_H 1

#include <stdint.h>

/* #define S_IFMT  0xF000 // File type mask
#define S_IFREG 0x8000 // Regular file
#define S_IFDIR 0x4000 // Directory */

#define S_IRUSR 0b100000000 // Read permission, owner
#define S_IWUSR 0b010000000 // Write permission, owner
#define S_IXUSR 0b001000000 // Execute/search permission, owner

#define S_IRGRP 0b000100000 // Read permission, group
#define S_IWGRP 0b000010000 // Write permission, group
#define S_IXGRP 0b000001000 // Execute/search permission, group

#define S_IROTH 0b000000100 // Read permission, others
#define S_IWOTH 0b000000010 // Write permission, others
#define S_IXOTH 0b000000001 // Execute/search permission, others

#endif /* _PLENJOS_STAT_H */