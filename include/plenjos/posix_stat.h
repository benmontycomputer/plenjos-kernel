#ifndef _PLENJOS_POSIX_STAT_H
#define _PLENJOS_POSIX_STAT_H 1

#include "plenjos/posix_types.h"

#define S_IFMT  0xF000 // File type mask
#define S_IFREG 0x8000 // Regular file
#define S_IFDIR 0x4000 // Directory
#define S_IFCHR 0x2000 // Character device
#define S_IFBLK 0x6000 // Block device
#define S_IFIFO 0x1000 // FIFO or pipe
#define S_IFLNK 0xA000 // Symbolic link
#define S_IFSOCK 0xC000 // Socket

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

#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR) // Read, write, execute/search by owner
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP) // Read, write, execute/search by group
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH) // Read, write, execute/search by others

#endif /* _PLENJOS_POSIX_STAT_H */