#pragma once
#include "stdint.h"
#include "disk.h"
#include "MM/heap.h"

typedef struct{
    uint8_t jmp[3];
    uint8_t oem_iden[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t fat_count;
    uint16_t root_entries_count;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads_count;
    uint32_t hidden_sectors_count;
    uint32_t large_sector_count;
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t info_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t signature;
    uint32_t volume_serial;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} __attribute__((packed)) FAT32_BPB_T;

typedef struct{
    uint32_t free_count;
    uint32_t next_free;
} FS_INFO_T;

typedef struct {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t created_time_tenths;
    uint16_t created_time;
    uint16_t created_date;
    uint16_t accesed_date;
    uint16_t first_cluster_high;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) DIR_ENTRY_T;

void init_fat32(void);
void read_file(const char* name, uint8_t* buffer);
uint32_t sizeof_file(const char* name);