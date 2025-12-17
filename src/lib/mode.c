#include "lib/mode.h"

#include <stdint.h>

access_t access_check(mode_t original_mode, uid_t file_uid, gid_t file_gid, uid_t process_uid) {
    access_t access = 0;

    if (process_uid == 0) {
        // Root can read and write anything, and execute anything that has at least one execute bit set
        access |= ACCESS_READ | ACCESS_WRITE;
        if (original_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            access |= ACCESS_EXECUTE;
        }
        return access;
    }

    if (process_uid == file_uid) {
        // Owner
        if (original_mode & S_IRUSR) {
            access |= ACCESS_READ;
        }
        if (original_mode & S_IWUSR) {
            access |= ACCESS_WRITE;
        }
        if (original_mode & S_IXUSR) {
            access |= ACCESS_EXECUTE;
        }
    } else {
        // Others
        if (original_mode & S_IROTH) {
            access |= ACCESS_READ;
        }
        if (original_mode & S_IWOTH) {
            access |= ACCESS_WRITE;
        }
        if (original_mode & S_IXOTH) {
            access |= ACCESS_EXECUTE;
        }
    }

    return access;
}