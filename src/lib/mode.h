#pragma once

#include "plenjos/stat.h"
#include "plenjos/types.h"

typedef mode_t access_t;

#define ACCESS_READ 0b100
#define ACCESS_WRITE 0b010
#define ACCESS_EXECUTE 0b001

// TODO: implement groups
access_t access_check(mode_t original_mode, uid_t file_uid, gid_t file_gid, uid_t process_uid);