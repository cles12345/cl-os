#include "fat32.h"

FAT32_BPB_T bpb;
static bool fat32_initialized = false;
static uint32_t last_free_cluster = 2;

static void filename_to_fat32(const char* normal, char* fat32_name)
{
    for (int i = 0; i < 11; i++) fat32_name[i] = ' ';
    
    const char* dot = strchr(normal, '.');
    int name_len = dot ? (int)(dot - normal) : (int)strlen(normal);
    int ext_len = dot ? (int)strlen(dot + 1) : 0;
    
    if (name_len > 8) name_len = 8;
    if (ext_len > 3) ext_len = 3;
    
    for (int i = 0; i < name_len; i++)
    {
        char c = normal[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat32_name[i] = c;
    }
    
    if (dot && ext_len > 0)
    {
        for (int i = 0; i < ext_len; i++)
        {
            char c = dot[1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            fat32_name[8 + i] = c;
        }
    }
}

static uint32_t get_next_cluster(uint32_t cluster)
{
    uint32_t fat_start = bpb.reserved_sector_count;
    uint8_t* fat_buf = kmalloc(bpb.bytes_per_sector);
    uint32_t fat_sector = fat_start + (cluster * 4 / bpb.bytes_per_sector);
    uint32_t fat_offset = (cluster * 4) % bpb.bytes_per_sector;
    
    read_sectors(fat_sector, 1, fat_buf);
    uint32_t next = *(uint32_t*)(fat_buf + fat_offset) & 0x0FFFFFFF;
    kfree(fat_buf);
    return next;
}

static void set_next_cluster(uint32_t cluster, uint32_t next_cluster)
{
    uint32_t fat_start = bpb.reserved_sector_count;
    uint8_t* fat_buf = kmalloc(bpb.bytes_per_sector);
    
    for (uint8_t f = 0; f < bpb.fat_count; f++)
    {
        uint32_t fat_table_offset = f * bpb.sectors_per_fat;
        uint32_t fat_sector = fat_start + fat_table_offset + (cluster * 4 / bpb.bytes_per_sector);
        uint32_t fat_offset = (cluster * 4) % bpb.bytes_per_sector;

        read_sectors(fat_sector, 1, fat_buf);
        uint32_t current_val = *(uint32_t*)(fat_buf + fat_offset);
        
        *(uint32_t*)(fat_buf + fat_offset) = (current_val & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
        
        write_sectors(fat_sector, 1, fat_buf);
    }
    kfree(fat_buf);
}

static uint32_t find_free_cluster(void)
{
    uint32_t fat_start = bpb.reserved_sector_count;
    uint32_t total_sectors = bpb.total_sectors ? bpb.total_sectors : bpb.large_sector_count;
    uint32_t total_clusters = (total_sectors - bpb.reserved_sector_count - 
                              (bpb.fat_count * bpb.sectors_per_fat)) / bpb.sectors_per_cluster;
    
    uint8_t* fat_buf = kmalloc(bpb.bytes_per_sector);
    
    for (uint32_t i = 0; i < total_clusters; i++)
    {
        uint32_t cluster = last_free_cluster;
        last_free_cluster++;
        if (last_free_cluster >= total_clusters + 2) last_free_cluster = 2;

        uint32_t fat_sector = fat_start + (cluster * 4 / bpb.bytes_per_sector);
        uint32_t fat_offset = (cluster * 4) % bpb.bytes_per_sector;
        read_sectors(fat_sector, 1, fat_buf);
        
        if ((*(uint32_t*)(fat_buf + fat_offset) & 0x0FFFFFFF) == 0)
        {
            kfree(fat_buf);
            return cluster;
        }
    }
    
    kfree(fat_buf);
    return 0;
}

static bool find_entry_in_cluster(uint32_t cluster, const char* name, DIR_ENTRY_T* out_entry, uint32_t* out_cluster)
{
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t entries_per_cluster = bytes_per_cluster / sizeof(DIR_ENTRY_T);
    uint32_t fat_start = bpb.reserved_sector_count;
    uint32_t data_start = fat_start + (bpb.fat_count * bpb.sectors_per_fat);
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    uint32_t current = cluster;
    
    while (current >= 2 && current < 0x0FFFFFF8)
    {
        uint32_t lba = data_start + ((current - 2) * bpb.sectors_per_cluster);
        read_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
        DIR_ENTRY_T* entries = (DIR_ENTRY_T*)cluster_buf;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++)
        {
            if (entries[i].name[0] == 0x00)
            {
                kfree(cluster_buf);
                return false;
            }
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if ((entries[i].attributes & 0x0F) == 0x0F) continue;
            
            if (memcmp(name, entries[i].name, 11) == 0)
            {
                if (out_entry) memcpy(out_entry, &entries[i], sizeof(DIR_ENTRY_T));
                if (out_cluster) *out_cluster = current;
                kfree(cluster_buf);
                return true;
            }
        }
        current = get_next_cluster(current);
    }
    
    kfree(cluster_buf);
    return false;
}

static bool find_file(const char* path, DIR_ENTRY_T* out_entry, uint32_t* parent_cluster)
{
    if (!fat32_initialized) init_fat32();

    if (parent_cluster) *parent_cluster = bpb.root_cluster;

    char path_copy[256];
    const char* src = path;
    if (*src == '/') src++;
    strcpy(path_copy, src);
    uint32_t len = strlen(path_copy);
    while (len > 0 && path_copy[len - 1] == '/') {
        path_copy[--len] = '\0';
    }

    if (len == 0)
    {
        if (out_entry)
        {
            memset(out_entry, 0, sizeof(DIR_ENTRY_T));
            out_entry->attributes = 0x10;
            out_entry->first_cluster_low = bpb.root_cluster & 0xFFFF;
            out_entry->first_cluster_high = (bpb.root_cluster >> 16) & 0xFFFF;
        }
        return true;
    }

    uint32_t current_cluster = bpb.root_cluster;
    char* token_start = path_copy;

    while (*token_start)
    {
        char* slash = strchr(token_start, '/');
        if (slash) *slash = '\0';

        if (strlen(token_start) > 0)
        {
            char fat32_name[12];
            filename_to_fat32(token_start, fat32_name);

            uint32_t dummy_cluster;
            DIR_ENTRY_T entry;
            if (!find_entry_in_cluster(current_cluster, fat32_name, &entry, &dummy_cluster)) return false;

            if (!slash || *(slash + 1) == '\0')
            {
                if (parent_cluster) *parent_cluster = current_cluster;
                if (out_entry) memcpy(out_entry, &entry, sizeof(DIR_ENTRY_T));
                return true;
            }

            if (!(entry.attributes & 0x10)) return false;

            current_cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
        }

        if (!slash) break;
        token_start = slash + 1;
    }

    return false;
}

static uint32_t resolve_parent_directory(const char* path, char* out_filename)
{
    char path_copy[256];
    const char* src = path;
    if (*src == '/') src++;
    strcpy(path_copy, src);

    unsigned int len = strlen(path_copy);
    if (len > 0 && path_copy[len - 1] == '/') path_copy[len - 1] = '\0';

    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash)
    {
        if (out_filename) strcpy(out_filename, path_copy);
        return bpb.root_cluster;
    }

    if (out_filename) strcpy(out_filename, last_slash + 1);
    *last_slash = '\0';

    DIR_ENTRY_T dir_entry;
    if (!find_file(path_copy, &dir_entry, 0)) return 0;
    if (!(dir_entry.attributes & 0x10)) return 0;

    return dir_entry.first_cluster_low | (dir_entry.first_cluster_high << 16);
}

static bool create_dir_entry(uint32_t cluster, const char* name, uint8_t attrs, uint32_t first_cluster, uint32_t size)
{
    if (cluster == 0) cluster = bpb.root_cluster;

    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t entries_per_cluster = bytes_per_cluster / sizeof(DIR_ENTRY_T);
    uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    uint32_t current = cluster;
    uint32_t last_cluster = current;

    while (current >= 2 && current < 0x0FFFFFF8)
    {
        uint32_t lba = data_start + ((current - 2) * bpb.sectors_per_cluster);
        read_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
        DIR_ENTRY_T* entries = (DIR_ENTRY_T*)cluster_buf;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++)
        {
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5)
            {
                char fat32_name[12];
                filename_to_fat32(name, fat32_name);
                memcpy(entries[i].name, fat32_name, 11);
                entries[i].attributes = attrs;
                entries[i].reserved = 0;
                entries[i].created_time_tenths = 0;
                entries[i].created_time = 0;
                entries[i].created_date = 0;
                entries[i].accesed_date = 0;
                entries[i].first_cluster_high = (first_cluster >> 16) & 0xFFFF;
                entries[i].modified_time = 0;
                entries[i].modified_date = 0;
                entries[i].first_cluster_low = first_cluster & 0xFFFF;
                entries[i].size = size;
                
                write_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
                kfree(cluster_buf);
                return true;
            }
        }
        last_cluster = current;
        current = get_next_cluster(current);
    }
    
    uint32_t new_cluster = find_free_cluster();
    if (!new_cluster) {
        kfree(cluster_buf);
        return false;
    }
    
    set_next_cluster(last_cluster, new_cluster);
    set_next_cluster(new_cluster, 0x0FFFFFF8);
    
    memset(cluster_buf, 0, bytes_per_cluster);
    DIR_ENTRY_T* entries = (DIR_ENTRY_T*)cluster_buf;
    
    char fat32_name[12];
    filename_to_fat32(name, fat32_name);
    memcpy(entries[0].name, fat32_name, 11);
    entries[0].attributes = attrs;
    entries[0].first_cluster_high = (first_cluster >> 16) & 0xFFFF;
    entries[0].first_cluster_low = first_cluster & 0xFFFF;
    entries[0].size = size;
    
    uint32_t lba = data_start + ((new_cluster - 2) * bpb.sectors_per_cluster);
    write_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
    kfree(cluster_buf);
    return true;
}

void init_fat32(void)
{
    if (fat32_initialized) return;
    fat32_initialized = true;
    uint8_t* buffer = kmalloc(512);
    read_sectors(0, 1, buffer);
    memcpy(&bpb, buffer, sizeof(FAT32_BPB_T));
    kfree(buffer);
}

uint32_t fat32_read_file(const char* path, uint8_t* buffer)
{
    DIR_ENTRY_T entry;
    if (!find_file(path, &entry, 0)) return 0;
    
    uint32_t cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
    uint32_t size = entry.size;
    
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
    uint32_t total_read = 0;
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    
    while (cluster >= 2 && cluster < 0x0FFFFFF8 && total_read < size)
    {
        uint32_t lba = data_start + ((cluster - 2) * bpb.sectors_per_cluster);
        read_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
        uint32_t remaining = size - total_read;
        uint32_t chunk = (remaining < bytes_per_cluster) ? remaining : bytes_per_cluster;
        memcpy(buffer + total_read, cluster_buf, chunk);
        total_read += chunk;
        cluster = get_next_cluster(cluster);
    }
    
    kfree(cluster_buf);
    return total_read;
}

uint32_t fat32_write_file(const char* path, const uint8_t* buffer, uint32_t size)
{
    char filename[256];
    uint32_t parent_cluster = resolve_parent_directory(path, filename);
    if (!parent_cluster) return 0;

    fat32_delete(path);
    
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t clusters_needed = (size + bytes_per_cluster - 1) / bytes_per_cluster;
    if (clusters_needed == 0) clusters_needed = 1;

    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;
    
    for (uint32_t i = 0; i < clusters_needed; i++)
    {
        uint32_t cluster = find_free_cluster();
        if (!cluster) return 0;
        
        if (i == 0) first_cluster = cluster;
        if (prev_cluster) set_next_cluster(prev_cluster, cluster);
        
        prev_cluster = cluster;
    }
    if (prev_cluster) set_next_cluster(prev_cluster, 0x0FFFFFF8);
    
    uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
    uint32_t cluster = first_cluster;
    uint32_t written = 0;
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    
    while (cluster >= 2 && cluster < 0x0FFFFFF8 && written < size)
    {
        uint32_t lba = data_start + ((cluster - 2) * bpb.sectors_per_cluster);
        uint32_t remaining = size - written;
        uint32_t chunk = (remaining < bytes_per_cluster) ? remaining : bytes_per_cluster;
        
        memset(cluster_buf, 0, bytes_per_cluster);
        memcpy((void*)cluster_buf, (void*)buffer + written, chunk);
        write_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
        
        written += chunk;
        cluster = get_next_cluster(cluster);
    }
    
    kfree(cluster_buf);
    
    create_dir_entry(parent_cluster, filename, 0x20, first_cluster, size);
    return written;
}

uint32_t fat32_file_size(const char* path)
{
    DIR_ENTRY_T entry;
    if (!find_file(path, &entry, 0)) return 0;
    return entry.size;
}

bool fat32_file_exists(const char* path)
{
    DIR_ENTRY_T entry;
    return find_file(path, &entry, 0);
}

bool fat32_create_directory(const char* path)
{
    DIR_ENTRY_T entry;
    if (find_file(path, &entry, 0)) return false;
    
    char dirname[256];
    uint32_t parent_cluster = resolve_parent_directory(path, dirname);
    if (!parent_cluster) return false;
    
    uint32_t cluster = find_free_cluster();
    if (!cluster) return false;
    
    set_next_cluster(cluster, 0x0FFFFFF8);
    
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    memset(cluster_buf, 0, bytes_per_cluster);
    DIR_ENTRY_T* entries = (DIR_ENTRY_T*)cluster_buf;
    
    memcpy(entries[0].name, ".          ", 11);
    entries[0].attributes = 0x10;
    entries[0].first_cluster_low = cluster & 0xFFFF;
    entries[0].first_cluster_high = (cluster >> 16) & 0xFFFF;
    
    uint32_t parent_dir_cluster = (parent_cluster == 0) ? bpb.root_cluster : parent_cluster;
    memcpy(entries[1].name, "..         ", 11);
    entries[1].attributes = 0x10;
    entries[1].first_cluster_low = parent_dir_cluster & 0xFFFF;
    entries[1].first_cluster_high = (parent_dir_cluster >> 16) & 0xFFFF;
    
    write_sectors(data_start + ((cluster - 2) * bpb.sectors_per_cluster), bpb.sectors_per_cluster, cluster_buf);
    kfree(cluster_buf);
    
    create_dir_entry(parent_cluster, dirname, 0x10, cluster, 0);
    return true;
}

bool fat32_list_directory(const char* path)
{
    DIR_ENTRY_T entry;
    if (!find_file(path, &entry, 0))
    {
        print("Directory not found\n");
        return false;
    }
    
    if (!(entry.attributes & 0x10))
    {
        print("Not a directory\n");
        return false;
    }
    
    uint32_t dir_cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
    if (dir_cluster == 0) dir_cluster = bpb.root_cluster;
    
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t entries_per_cluster = bytes_per_cluster / sizeof(DIR_ENTRY_T);
    uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    
    print("Contents:\n");
    while (dir_cluster >= 2 && dir_cluster < 0x0FFFFFF8)
    {
        uint32_t lba = data_start + ((dir_cluster - 2) * bpb.sectors_per_cluster);
        read_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
        DIR_ENTRY_T* entries = (DIR_ENTRY_T*)cluster_buf;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++)
        {
            if (entries[i].name[0] == 0x00) break;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if ((entries[i].attributes & 0x0F) == 0x0F) continue;
            
            char name[12];
            memcpy(name, entries[i].name, 11);
            name[11] = '\0';
            
            if (entries[i].attributes & 0x10) print("[DIR]  ");
            else print("[FILE] ");
            print(name);
            if (!(entries[i].attributes & 0x10))
            {
                print("  (");
                printi(entries[i].size);
                print(" bytes)");
            }
            print("\n");
        }
        dir_cluster = get_next_cluster(dir_cluster);
    }
    
    kfree(cluster_buf);
    return true;
}

bool fat32_delete(const char* path)
{
    uint32_t parent_cluster;
    DIR_ENTRY_T entry;
    if (!find_file(path, &entry, &parent_cluster)) return false;
    
    if (entry.attributes & 0x10) {
        uint32_t dir_cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
        uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
        uint32_t entries_per_cluster = bytes_per_cluster / sizeof(DIR_ENTRY_T);
        uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
        uint8_t* check_buf = kmalloc(bytes_per_cluster);
        uint32_t cur = dir_cluster;
        bool has_entries = false;
        
        while (cur >= 2 && cur < 0x0FFFFFF8) {
            uint32_t lba = data_start + ((cur - 2) * bpb.sectors_per_cluster);
            read_sectors(lba, bpb.sectors_per_cluster, check_buf);
            DIR_ENTRY_T* dir_entries = (DIR_ENTRY_T*)check_buf;
            for (uint32_t i = 0; i < entries_per_cluster; i++) {
                if (dir_entries[i].name[0] == 0x00) break;
                if ((uint8_t)dir_entries[i].name[0] == 0xE5) continue;
                if (memcmp(dir_entries[i].name, ".          ", 11) == 0 ||
                    memcmp(dir_entries[i].name, "..         ", 11) == 0)
                    continue;
                has_entries = true;
                break;
            }
            if (has_entries) break;
            cur = get_next_cluster(cur);
        }
        kfree(check_buf);
        if (has_entries) return false;
    }

    uint32_t cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);

    while (cluster >= 2 && cluster < 0x0FFFFFF8)
    {
        uint32_t next = get_next_cluster(cluster);
        set_next_cluster(cluster, 0x00000000);
        cluster = next;
    }

    char target_name[256];
    resolve_parent_directory(path, target_name);

    char fat32_name[12];
    filename_to_fat32(target_name, fat32_name);

    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t entries_per_cluster = bytes_per_cluster / sizeof(DIR_ENTRY_T);
    uint32_t data_start = bpb.reserved_sector_count + (bpb.fat_count * bpb.sectors_per_fat);
    uint8_t* cluster_buf = kmalloc(bytes_per_cluster);
    uint32_t curr_parent = (parent_cluster == 0) ? bpb.root_cluster : parent_cluster;

    while (curr_parent >= 2 && curr_parent < 0x0FFFFFF8)
    {
        uint32_t lba = data_start + ((curr_parent - 2) * bpb.sectors_per_cluster);
        read_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
        DIR_ENTRY_T* entries = (DIR_ENTRY_T*)cluster_buf;

        for (uint32_t i = 0; i < entries_per_cluster; i++)
        {
            if (entries[i].name[0] == 0x00) break;
            if (memcmp(fat32_name, entries[i].name, 11) == 0)
            {
                entries[i].name[0] = 0xE5;
                write_sectors(lba, bpb.sectors_per_cluster, cluster_buf);
                kfree(cluster_buf);
                return true;
            }
        }
        curr_parent = get_next_cluster(curr_parent);
    }

    kfree(cluster_buf);
    return true;
}