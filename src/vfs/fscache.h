#pragma once

#include "vfs/fscache.h"

#include "vfs/vfs.h"
#include "vfs/kernelfs.h"
#include "proc/proc.h"

#include "plenjos/limits.h"
#include "plenjos/stat.h"
#include "plenjos/types.h"

#include <stdint.h>

#define FSCACHE_FLAG_DIRTY       0x1
#define FSCACHE_FLAG_MOUNT_POINT 0x2

typedef uint16_t fscache_flags_t;

typedef struct fscache_node fscache_node_t;

struct fscache_node {
    char name[NAME_MAX + 1];

    dirent_type_t type;
    fscache_flags_t flags;

    uid_t uid;
    gid_t gid;
    mode_t mode;

    fscache_node_t *parent;
    fscache_node_t *loaded_children;
    fscache_node_t *prev;
    fscache_node_t *next;
} __attribute__((packed));