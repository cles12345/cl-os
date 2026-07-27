#include "fat.h"

FAT32_BPB_T bpb;
DIR_ENTRY_T *root_dir;
static bool fat32_initialized = false;

static DIR_ENTRY_T* find_file(const char* name){
    if (!fat32_initialized) init_fat32();
    uint32_t entries_per_cluster = (bpb.sectors_per_cluster * bpb.bytes_per_sector) / sizeof(DIR_ENTRY_T);
    
    for (uint32_t i = 0; i < entries_per_cluster; i++){
        if (root_dir[i].name[0] == 0x00)
            break;
            
        if (root_dir[i].name[0] == 0xE5)
            continue;
            
        if  (memcmp(name, root_dir[i].name, 11)){
            return &root_dir[i];
        }
    }
    return 0;
}

static void read_root_dir(void){
    if (!fat32_initialized) init_fat32();
    uint32_t fat_start = bpb.reserved_sector_count;
    uint32_t data_start = fat_start + (bpb.fat_count * bpb.sectors_per_fat);

    uint32_t root_cluster = bpb.root_cluster;
    uint32_t lba = data_start + ((root_cluster - 2) * bpb.sectors_per_cluster);

    root_dir = kmalloc(bpb.sectors_per_cluster * bpb.bytes_per_sector);
    read_sectors(lba, bpb.sectors_per_cluster, root_dir);
}

void read_file(const char* name, uint8_t* buffer){
    if (!fat32_initialized) init_fat32();
    DIR_ENTRY_T* entry = find_file(name);
    if (!entry){
        print("couldnt find the file\n");
        return;
    }

    uint32_t cluster = entry->first_cluster_low | (entry->first_cluster_high << 16);
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t fat_start = bpb.reserved_sector_count;
    uint32_t data_start = fat_start + (bpb.fat_count * bpb.sectors_per_fat);
    uint32_t total_read = 0;
    
    uint8_t* fat_buf = kmalloc(bpb.bytes_per_sector);
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    while (cluster < 0x0FFFFFF8 && total_read < entry->size) {
        uint32_t lba = data_start + ((cluster - 2) * bpb.sectors_per_cluster);
        read_sectors(lba, bpb.sectors_per_cluster, cluster_buf);

        uint32_t remaining = entry->size - total_read;
        uint32_t chunk = (remaining < bytes_per_cluster) ? remaining : bytes_per_cluster;
        
        memcpy(buffer + total_read, cluster_buf, chunk);
        total_read += chunk;

        uint32_t fat_sector = fat_start + (cluster * 4 / bpb.bytes_per_sector);
        uint32_t fat_offset = (cluster * 4) % bpb.bytes_per_sector;
        read_sectors(fat_sector, 1, fat_buf);
        cluster = *(uint32_t*)(fat_buf + fat_offset) & 0x0FFFFFFF;
    }
    kfree(fat_buf);
    kfree(cluster_buf);
}

uint32_t sizeof_file(const char* name){
    if (!fat32_initialized) init_fat32();
    DIR_ENTRY_T* entry = find_file(name);
    if (!entry){
        print("couldnt find the file\n");
        return 0;
    }
    return entry->size;
}

void init_fat32(void){
    fat32_initialized = true;
    uint8_t *buffer = kmalloc(512);
    read_sectors(0,  1, buffer);
    memcpy(&bpb, buffer, sizeof(FAT32_BPB_T));
    kfree(buffer);
    read_root_dir();
}