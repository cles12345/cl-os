#pragma once
#include "stdint.h"
#include "utill.h"

#define ATA_DATA_PORT 0x1F0
#define ATA_SECTOR_COUNT_PORT 0x1F2
#define ATA_LBA_LOW_PORT 0x1F3
#define ATA_LBA_MID_PORT 0x1F4
#define ATA_LBA_HIGH_PORT 0x1F5
#define ATA_DRIVE_PORT 0x1F6
#define ATA_STATUS_PORT 0x1F7

void read_sectors(uint32_t lba, uint32_t sector_count, uint8_t* buffer);
void write_sectors(uint32_t lba, uint32_t sector_count, uint8_t* buffer);