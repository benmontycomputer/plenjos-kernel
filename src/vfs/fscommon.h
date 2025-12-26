#pragma once

#include "devices/storage/drive.h"

int read_first_sector(DRIVE_t *drive, uint32_t partition_start_lba, uint8_t *buffer);