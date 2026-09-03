#include "ext3.h"

static EXT3_SUPERBLOCK_T sb;
static bool ext3_initialized = false;
static uint32_t block_size = 1024;
static uint32_t inode_size = 128;
static uint32_t inodes_per_group = 0;
static uint32_t blocks_per_group = 0;
static uint32_t group_count = 0;
static uint32_t first_data_block = 0;
static uint32_t last_allocated_inode = 2;
static uint32_t last_allocated_block = 0;
static uint32_t partition_offset = 0;

static uint32_t block_to_lba(uint32_t block)
{
    if (block_size == 0) return 0;
    return (partition_offset + block) * (block_size / 512);
}

static uint32_t inode_to_block(uint32_t inode)
{
    if (inodes_per_group == 0 || block_size == 0) return 0;
    
    uint32_t group = (inode - 1) / inodes_per_group;
    uint32_t index = (inode - 1) % inodes_per_group;
    uint32_t gd_block = 2;
    uint8_t* buf = kmalloc(block_size);
    if (!buf) return 0;
    
    read_sectors(block_to_lba(gd_block), block_size / 512, buf);
    EXT3_GROUP_DESC_T* gd = (EXT3_GROUP_DESC_T*)(buf + (group * 32));
    uint32_t table = gd->inode_table;
    uint32_t offset = index * inode_size;
    kfree(buf);
    return table + (offset / block_size);
}

static uint32_t inode_offset(uint32_t inode)
{
    if (inodes_per_group == 0 || block_size == 0) return 0;
    
    uint32_t group = (inode - 1) / inodes_per_group;
    uint32_t index = (inode - 1) % inodes_per_group;
    return (index * inode_size) % block_size;
}

static void read_inode(uint32_t inode, EXT3_INODE_T* out)
{
    if (!out || !ext3_initialized) return;
    
    uint32_t block = inode_to_block(inode);
    if (!block) return;
    
    uint32_t offset = inode_offset(inode);
    uint8_t* buf = kmalloc(block_size);
    if (!buf) return;
    
    read_sectors(block_to_lba(block), block_size / 512, buf);
    memcpy(out, buf + offset, inode_size);
    kfree(buf);
}

static void write_inode(uint32_t inode, EXT3_INODE_T* in)
{
    if (!in || !ext3_initialized) return;
    
    uint32_t block = inode_to_block(inode);
    if (!block) return;
    
    uint32_t offset = inode_offset(inode);
    uint8_t* buf = kmalloc(block_size);
    if (!buf) return;
    
    read_sectors(block_to_lba(block), block_size / 512, buf);
    memcpy(buf + offset, in, inode_size);
    write_sectors(block_to_lba(block), block_size / 512, buf);
    kfree(buf);
}

static uint32_t read_block(uint32_t block, uint8_t* buffer)
{
    if (!buffer || !ext3_initialized) return 0;
    read_sectors(block_to_lba(block), block_size / 512, buffer);
    return block_size;
}

static void write_block(uint32_t block, uint8_t* buffer)
{
    if (!buffer || !ext3_initialized) return;
    write_sectors(block_to_lba(block), block_size / 512, buffer);
}

static uint32_t find_free_inode(void)
{
    if (!ext3_initialized) return 0;
    
    for (uint32_t g = 0; g < group_count; g++) {
        uint32_t gd_block = 2 + (g * 2);
        uint8_t* buf = kmalloc(block_size);
        if (!buf) return 0;
        
        read_sectors(block_to_lba(gd_block), block_size / 512, buf);
        EXT3_GROUP_DESC_T* gd = (EXT3_GROUP_DESC_T*)(buf + (g % (block_size/32)) * 32);
        
        if (gd->free_inodes_count > 0) {
            uint32_t bitmap = gd->inode_bitmap;
            read_sectors(block_to_lba(bitmap), block_size / 512, buf);
            
            uint32_t start = (g == 0) ? 2 : 0;
            for (uint32_t i = start; i < inodes_per_group; i++) {
                uint32_t byte = i / 8;
                uint32_t bit = i % 8;
                if (!(buf[byte] & (1 << bit))) {
                    uint32_t inode = g * inodes_per_group + i + 1;
                    buf[byte] |= (1 << bit);
                    write_sectors(block_to_lba(bitmap), block_size / 512, buf);
                    kfree(buf);
                    return inode;
                }
            }
        }
        kfree(buf);
    }
    return 0;
}

static uint32_t find_free_block(void)
{
    if (!ext3_initialized) return 0;
    
    if (last_allocated_block == 0) last_allocated_block = first_data_block;
    
    for (uint32_t g = 0; g < group_count; g++) {
        uint32_t gd_block = 2 + (g * 2);
        uint8_t* buf = kmalloc(block_size);
        if (!buf) return 0;
        
        read_sectors(block_to_lba(gd_block), block_size / 512, buf);
        EXT3_GROUP_DESC_T* gd = (EXT3_GROUP_DESC_T*)(buf + (g % (block_size/32)) * 32);
        
        if (gd->free_blocks_count > 0) {
            uint32_t bitmap = gd->block_bitmap;
            read_sectors(block_to_lba(bitmap), block_size / 512, buf);
            
            uint32_t start = (g == 0) ? first_data_block : 0;
            for (uint32_t i = start; i < blocks_per_group; i++) {
                uint32_t byte = i / 8;
                uint32_t bit = i % 8;
                if (!(buf[byte] & (1 << bit))) {
                    uint32_t block = g * blocks_per_group + i;
                    if (block < first_data_block) continue;
                    buf[byte] |= (1 << bit);
                    write_sectors(block_to_lba(bitmap), block_size / 512, buf);
                    kfree(buf);
                    return block;
                }
            }
        }
        kfree(buf);
    }
    return 0;
}

static void free_block(uint32_t block)
{
    if (!ext3_initialized) return;
    
    uint32_t group = block / blocks_per_group;
    uint32_t index = block % blocks_per_group;
    uint32_t gd_block = 2 + (group * 2);
    uint8_t* buf = kmalloc(block_size);
    if (!buf) return;
    
    read_sectors(block_to_lba(gd_block), block_size / 512, buf);
    EXT3_GROUP_DESC_T* gd = (EXT3_GROUP_DESC_T*)(buf + (group % (block_size/32)) * 32);
    
    uint32_t bitmap = gd->block_bitmap;
    read_sectors(block_to_lba(bitmap), block_size / 512, buf);
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;
    buf[byte] &= ~(1 << bit);
    write_sectors(block_to_lba(bitmap), block_size / 512, buf);
    kfree(buf);
}

static uint32_t block_from_inode(EXT3_INODE_T* inode, uint32_t index)
{
    if (!inode || !ext3_initialized) return 0;
    
    if (index < 12) {
        return inode->block[index];
    }
    
    uint32_t entries = block_size / 4;
    if (entries == 0) return 0;
    
    index -= 12;
    if (index < entries) {
        uint8_t* buf = kmalloc(block_size);
        if (!buf) return 0;
        read_block(inode->block[12], buf);
        uint32_t result = ((uint32_t*)buf)[index];
        kfree(buf);
        return result;
    }
    
    index -= entries;
    if (index < entries * entries) {
        uint32_t ptr1 = index / entries;
        uint32_t ptr2 = index % entries;
        uint8_t* buf = kmalloc(block_size);
        if (!buf) return 0;
        read_block(inode->block[13], buf);
        uint32_t block = ((uint32_t*)buf)[ptr1];
        read_block(block, buf);
        uint32_t result = ((uint32_t*)buf)[ptr2];
        kfree(buf);
        return result;
    }
    
    return 0;
}

static void add_block_to_inode(EXT3_INODE_T* inode, uint32_t block)
{
    if (!inode) return;
    
    for (uint32_t i = 0; i < 12; i++) {
        if (inode->block[i] == 0) {
            inode->block[i] = block;
            inode->blocks += (block_size / 512);
            return;
        }
    }
}

static bool find_dir_entry(uint32_t inode, const char* name, EXT3_DIR_ENTRY_T* out)
{
    if (!ext3_initialized || !name) return false;
    
    EXT3_INODE_T in;
    read_inode(inode, &in);
    
    uint32_t size = in.size;
    uint8_t* buf = kmalloc(size + block_size);
    if (!buf) return false;
    memset(buf, 0, size + block_size);
    
    for (uint32_t i = 0; i < 12; i++) {
        if (in.block[i] == 0) break;
        read_block(in.block[i], buf + (i * block_size));
    }
    
    uint32_t offset = 0;
    while (offset < size) {
        EXT3_DIR_ENTRY_T* entry = (EXT3_DIR_ENTRY_T*)(buf + offset);
        if (entry->inode == 0) break;
        
        char entry_name[256];
        uint32_t name_len = entry->name_len;
        if (name_len > 255) name_len = 255;
        memcpy(entry_name, entry->name, name_len);
        entry_name[name_len] = '\0';
        
        if (strcmp(name, entry_name) == 0) {
            if (out) memcpy(out, entry, sizeof(EXT3_DIR_ENTRY_T));
            kfree(buf);
            return true;
        }
        offset += entry->rec_len;
        if (offset >= size) break;
    }
    
    kfree(buf);
    return false;
}

static bool find_file(const char* path, EXT3_INODE_T* out_inode, uint32_t* out_inode_num)
{
    if (!ext3_initialized) init_ext3();
    if (!ext3_initialized || !path) return false;
    
    char path_copy[256];
    const char* src = path;
    if (*src == '/') src++;
    strcpy(path_copy, src);
    uint32_t len = strlen(path_copy);
    while (len > 0 && path_copy[len - 1] == '/') {
        path_copy[--len] = '\0';
    }
    
    if (len == 0) {
        if (out_inode) read_inode(2, out_inode);
        if (out_inode_num) *out_inode_num = 2;
        return true;
    }
    
    uint32_t current_inode = 2;
    char* token_start = path_copy;
    
    while (*token_start) {
        char* slash = strchr(token_start, '/');
        if (slash) *slash = '\0';
        
        if (strlen(token_start) > 0) {
            EXT3_DIR_ENTRY_T entry;
            if (!find_dir_entry(current_inode, token_start, &entry)) {
                return false;
            }
            
            if (!slash || *(slash + 1) == '\0') {
                if (out_inode) read_inode(entry.inode, out_inode);
                if (out_inode_num) *out_inode_num = entry.inode;
                return true;
            }
            
            EXT3_INODE_T dir_inode;
            read_inode(entry.inode, &dir_inode);
            if (!(dir_inode.mode & 0x4000)) return false;
            
            current_inode = entry.inode;
        }
        if (!slash) break;
        token_start = slash + 1;
    }
    
    return false;
}

static uint32_t resolve_parent(const char* path, char* filename)
{
    if (!ext3_initialized || !path) return 0;
    
    char path_copy[256];
    const char* src = path;
    if (*src == '/') src++;
    strcpy(path_copy, src);
    
    uint32_t len = strlen(path_copy);
    if (len > 0 && path_copy[len - 1] == '/') path_copy[len - 1] = '\0';
    
    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        if (filename) strcpy(filename, path_copy);
        return 2;
    }
    
    if (filename) strcpy(filename, last_slash + 1);
    *last_slash = '\0';
    
    EXT3_INODE_T dir_inode;
    uint32_t dir_inode_num;
    if (!find_file(path_copy, &dir_inode, &dir_inode_num)) return 0;
    if (!(dir_inode.mode & 0x4000)) return 0;
    
    return dir_inode_num;
}

static bool create_dir_entry(uint32_t parent_inode, const char* name, uint32_t inode_num, uint8_t type)
{
    if (!ext3_initialized || !name) return false;
    
    EXT3_INODE_T parent;
    read_inode(parent_inode, &parent);
    
    uint32_t size = parent.size;
    uint32_t blocks_needed = (size + block_size) / block_size;
    uint8_t* buf = kmalloc((blocks_needed + 1) * block_size);
    if (!buf) return false;
    memset(buf, 0, (blocks_needed + 1) * block_size);
    
    for (uint32_t i = 0; i < 12; i++) {
        if (parent.block[i] == 0) break;
        read_block(parent.block[i], buf + (i * block_size));
    }
    
    uint32_t offset = 0;
    while (offset < size) {
        EXT3_DIR_ENTRY_T* entry = (EXT3_DIR_ENTRY_T*)(buf + offset);
        if (entry->inode == 0) break;
        offset += entry->rec_len;
    }
    
    uint32_t name_len = strlen(name);
    uint32_t rec_len = sizeof(EXT3_DIR_ENTRY_T) - 255 + name_len;
    rec_len = ((rec_len + 7) & ~7);
    
    EXT3_DIR_ENTRY_T* new_entry = (EXT3_DIR_ENTRY_T*)(buf + offset);
    new_entry->inode = inode_num;
    new_entry->rec_len = rec_len;
    new_entry->name_len = name_len;
    new_entry->file_type = type;
    memcpy(new_entry->name, name, name_len);
    
    parent.size += rec_len;
    
    uint32_t block_index = size / block_size;
    if (block_index < 12 && parent.block[block_index] == 0) {
        uint32_t new_block = find_free_block();
        if (!new_block) { kfree(buf); return false; }
        parent.block[block_index] = new_block;
        parent.blocks += (block_size / 512);
        write_inode(parent_inode, &parent);
        write_block(new_block, buf + (block_index * block_size));
    } else {
        write_block(parent.block[block_index], buf + (block_index * block_size));
    }
    
    write_inode(parent_inode, &parent);
    kfree(buf);
    return true;
}

void init_ext3(void)
{
    if (ext3_initialized) return;
    
    partition_offset = 0;
    
    uint8_t* buf = kmalloc(1024);
    read_sectors(2, 2, buf);
    memcpy(&sb, buf, sizeof(EXT3_SUPERBLOCK_T));
    
    if (sb.magic != 0xEF53) {
        kfree(buf);
        return;
    }
    
    block_size = 1024 << sb.log_block_size;
    inode_size = sb.inode_size;
    inodes_per_group = sb.inodes_per_group;
    blocks_per_group = sb.blocks_per_group;
    group_count = sb.blocks_count / blocks_per_group;
    first_data_block = sb.first_data_block;
    
    if (inodes_per_group == 0 || block_size == 0) {
        kfree(buf);
        return;
    }
    
    ext3_initialized = true;
    kfree(buf);
}

uint32_t ext3_read_file(const char* path, uint8_t* buffer)
{
    if (!ext3_initialized || !path || !buffer) return 0;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) return 0;
    
    if (inode.mode & 0xA000) {
        if (inode.size < 60) {
            memcpy(buffer, (uint8_t*)&inode.block[0], inode.size);
            return inode.size;
        } else {
            uint8_t* buf = kmalloc(block_size);
            if (!buf) return 0;
            read_block(inode.block[0], buf);
            memcpy(buffer, buf, inode.size);
            kfree(buf);
            return inode.size;
        }
    }
    
    uint32_t size = inode.size;
    uint32_t total_read = 0;
    uint32_t block_index = 0;
    
    while (total_read < size) {
        uint32_t block = block_from_inode(&inode, block_index);
        if (!block) break;
        
        uint32_t remaining = size - total_read;
        uint32_t chunk = (remaining < block_size) ? remaining : block_size;
        uint8_t* buf = kmalloc(block_size);
        if (!buf) break;
        read_block(block, buf);
        memcpy(buffer + total_read, buf, chunk);
        kfree(buf);
        total_read += chunk;
        block_index++;
    }
    
    return total_read;
}

uint32_t ext3_write_file(const char* path, const uint8_t* buffer, uint32_t size)
{
    if (!ext3_initialized || !path) return 0;
    
    char filename[256];
    uint32_t parent = resolve_parent(path, filename);
    if (!parent) return 0;
    
    ext3_delete(path);
    
    uint32_t inode_num = find_free_inode();
    if (!inode_num) return 0;
    
    EXT3_INODE_T inode;
    memset(&inode, 0, sizeof(EXT3_INODE_T));
    inode.mode = 0x81A4;
    inode.size = size;
    inode.links_count = 1;
    inode.blocks = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.atime = 0;
    inode.ctime = 0;
    inode.mtime = 0;
    
    uint32_t total_written = 0;
    uint32_t block_index = 0;
    
    while (total_written < size) {
        uint32_t block = find_free_block();
        if (!block) break;
        
        uint32_t remaining = size - total_written;
        uint32_t chunk = (remaining < block_size) ? remaining : block_size;
        uint8_t* buf = kmalloc(block_size);
        if (!buf) break;
        memset(buf, 0, block_size);
        memcpy(buf, buffer + total_written, chunk);
        write_block(block, buf);
        kfree(buf);
        
        add_block_to_inode(&inode, block);
        total_written += chunk;
        block_index++;
    }
    
    write_inode(inode_num, &inode);
    create_dir_entry(parent, filename, inode_num, 1);
    
    return total_written;
}

bool ext3_create_symlink(const char* target, const char* path)
{
    if (!ext3_initialized || !target || !path) return false;
    
    char filename[256];
    uint32_t parent = resolve_parent(path, filename);
    if (!parent) return false;
    
    ext3_delete(path);
    
    uint32_t inode_num = find_free_inode();
    if (!inode_num) return false;
    
    EXT3_INODE_T inode;
    memset(&inode, 0, sizeof(EXT3_INODE_T));
    inode.mode = 0xA1FF;
    inode.size = strlen(target);
    inode.links_count = 1;
    inode.blocks = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.atime = 0;
    inode.ctime = 0;
    inode.mtime = 0;
    
    if (inode.size < 60) {
        memcpy((uint8_t*)&inode.block[0], target, inode.size);
    } else {
        uint32_t block = find_free_block();
        if (!block) return false;
        uint8_t* buf = kmalloc(block_size);
        if (!buf) return false;
        memset(buf, 0, block_size);
        memcpy(buf, target, inode.size);
        write_block(block, buf);
        kfree(buf);
        inode.block[0] = block;
        inode.blocks = (block_size / 512);
    }
    
    write_inode(inode_num, &inode);
    create_dir_entry(parent, filename, inode_num, 7);
    return true;
}

char* ext3_read_symlink(const char* path)
{
    if (!ext3_initialized || !path) return NULL;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) return NULL;
    if (!(inode.mode & 0xA000)) return NULL;
    
    char* result = kmalloc(inode.size + 1);
    if (!result) return NULL;
    
    if (inode.size < 60) {
        memcpy(result, (uint8_t*)&inode.block[0], inode.size);
    } else {
        uint8_t* buf = kmalloc(block_size);
        if (!buf) { kfree(result); return NULL; }
        read_block(inode.block[0], buf);
        memcpy(result, buf, inode.size);
        kfree(buf);
    }
    result[inode.size] = '\0';
    return result;
}

bool ext3_is_symlink(const char* path)
{
    if (!ext3_initialized || !path) return false;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) return false;
    return (inode.mode & 0xA000) != 0;
}

uint32_t ext3_file_size(const char* path)
{
    if (!ext3_initialized || !path) return 0;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) return 0;
    return inode.size;
}

bool ext3_file_exists(const char* path)
{
    if (!ext3_initialized || !path) return false;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    return find_file(path, &inode, &inode_num);
}

bool ext3_create_directory(const char* path)
{
    if (!ext3_initialized || !path) return false;
    
    EXT3_INODE_T entry;
    if (find_file(path, &entry, 0)) return false;
    
    char dirname[256];
    uint32_t parent = resolve_parent(path, dirname);
    if (!parent) return false;
    
    uint32_t inode_num = find_free_inode();
    if (!inode_num) return false;
    
    uint32_t block = find_free_block();
    if (!block) return false;
    
    EXT3_INODE_T inode;
    memset(&inode, 0, sizeof(EXT3_INODE_T));
    inode.mode = 0x41ED;
    inode.size = block_size;
    inode.links_count = 2;
    inode.blocks = (block_size / 512);
    inode.uid = 0;
    inode.gid = 0;
    inode.atime = 0;
    inode.ctime = 0;
    inode.mtime = 0;
    inode.block[0] = block;
    
    uint8_t* buf = kmalloc(block_size);
    if (!buf) return false;
    memset(buf, 0, block_size);
    
    EXT3_DIR_ENTRY_T* dot = (EXT3_DIR_ENTRY_T*)buf;
    dot->inode = inode_num;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = 2;
    memcpy(dot->name, ".", 1);
    
    EXT3_DIR_ENTRY_T* dotdot = (EXT3_DIR_ENTRY_T*)(buf + 12);
    dotdot->inode = parent;
    dotdot->rec_len = block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    memcpy(dotdot->name, "..", 2);
    
    write_block(block, buf);
    kfree(buf);
    
    write_inode(inode_num, &inode);
    create_dir_entry(parent, dirname, inode_num, 2);
    
    return true;
}

bool ext3_list_directory(const char* path)
{
    if (!ext3_initialized || !path) return false;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) {
        print("Directory not found\n");
        return false;
    }
    
    if (!(inode.mode & 0x4000)) {
        print("Not a directory\n");
        return false;
    }
    
    uint32_t size = inode.size;
    uint8_t* buf = kmalloc(size + block_size);
    if (!buf) return false;
    memset(buf, 0, size + block_size);
    
    for (uint32_t i = 0; i < 12; i++) {
        if (inode.block[i] == 0) break;
        read_block(inode.block[i], buf + (i * block_size));
    }
    
    print("Contents:\n");
    uint32_t offset = 0;
    while (offset < size) {
        EXT3_DIR_ENTRY_T* entry = (EXT3_DIR_ENTRY_T*)(buf + offset);
        if (entry->inode == 0) break;
        
        char name[256];
        uint32_t name_len = entry->name_len;
        if (name_len > 255) name_len = 255;
        memcpy(name, entry->name, name_len);
        name[name_len] = '\0';
        
        if (name[0] == '.' && (name_len == 1 || (name_len == 2 && name[1] == '.'))) {
            offset += entry->rec_len;
            continue;
        }
        
        EXT3_INODE_T file_inode;
        read_inode(entry->inode, &file_inode);
        
        if (file_inode.mode & 0x4000) {
            print("[DIR]  ");
        } else if (file_inode.mode & 0xA000) {
            print("[LINK] ");
        } else {
            print("[FILE] ");
        }
        print(name);
        if (!(file_inode.mode & 0x4000) && !(file_inode.mode & 0xA000)) {
            print("  (");
            printi(file_inode.size);
            print(" bytes)");
        }
        print("\n");
        
        offset += entry->rec_len;
        if (offset >= size) break;
    }
    
    kfree(buf);
    return true;
}

bool ext3_delete(const char* path)
{
    if (!ext3_initialized || !path) return false;
    
    uint32_t parent_inode;
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) return false;
    
    if (inode.mode & 0x4000) {
        uint32_t size = inode.size;
        uint8_t* buf = kmalloc(size + block_size);
        if (!buf) return false;
        memset(buf, 0, size + block_size);
        
        for (uint32_t i = 0; i < 12; i++) {
            if (inode.block[i] == 0) break;
            read_block(inode.block[i], buf + (i * block_size));
        }
        
        uint32_t offset = 0;
        bool has_entries = false;
        while (offset < size) {
            EXT3_DIR_ENTRY_T* entry = (EXT3_DIR_ENTRY_T*)(buf + offset);
            if (entry->inode == 0) break;
            
            char name[256];
            uint32_t name_len = entry->name_len;
            if (name_len > 255) name_len = 255;
            memcpy(name, entry->name, name_len);
            name[name_len] = '\0';
            
            if (!(name[0] == '.' && (name_len == 1 || (name_len == 2 && name[1] == '.')))) {
                has_entries = true;
                break;
            }
            offset += entry->rec_len;
        }
        kfree(buf);
        
        if (has_entries) return false;
        
        for (uint32_t i = 0; i < 12; i++) {
            if (inode.block[i] == 0) break;
            free_block(inode.block[i]);
        }
    }
    
    if (inode.mode & 0xA000) {
        if (inode.size >= 60) {
            free_block(inode.block[0]);
        }
    }
    
    for (uint32_t i = 0; i < 12; i++) {
        if (inode.block[i] == 0) break;
        free_block(inode.block[i]);
    }
    
    char target_name[256];
    parent_inode = resolve_parent(path, target_name);
    
    EXT3_INODE_T parent;
    read_inode(parent_inode, &parent);
    
    uint32_t size = parent.size;
    uint8_t* buf = kmalloc(size + block_size);
    if (!buf) return false;
    memset(buf, 0, size + block_size);
    
    for (uint32_t i = 0; i < 12; i++) {
        if (parent.block[i] == 0) break;
        read_block(parent.block[i], buf + (i * block_size));
    }
    
    uint32_t offset = 0;
    while (offset < size) {
        EXT3_DIR_ENTRY_T* entry = (EXT3_DIR_ENTRY_T*)(buf + offset);
        if (entry->inode == 0) break;
        
        char name[256];
        uint32_t name_len = entry->name_len;
        if (name_len > 255) name_len = 255;
        memcpy(name, entry->name, name_len);
        name[name_len] = '\0';
        
        if (strcmp(target_name, name) == 0) {
            entry->inode = 0;
            write_block(parent.block[offset / block_size], buf + ((offset / block_size) * block_size));
            kfree(buf);
            return true;
        }
        offset += entry->rec_len;
    }
    
    kfree(buf);
    return true;
}

EXT3_FILE_T* ext3_open(const char* path, uint32_t flags)
{
    if (!ext3_initialized || !path) return NULL;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    
    if (!find_file(path, &inode, &inode_num)) {
        if (flags & 0x02) {
            if (!ext3_write_file(path, NULL, 0)) return NULL;
            if (!find_file(path, &inode, &inode_num)) return NULL;
        } else {
            return NULL;
        }
    }
    
    if (inode.mode & 0xA000) {
        char* target = ext3_read_symlink(path);
        if (target) {
            EXT3_FILE_T* result = ext3_open(target, flags);
            kfree(target);
            return result;
        }
        return NULL;
    }
    
    EXT3_FILE_T* file = kmalloc(sizeof(EXT3_FILE_T));
    if (!file) return NULL;
    
    file->inode_num = inode_num;
    file->position = 0;
    file->size = inode.size;
    file->flags = flags;
    memcpy(&file->inode, &inode, sizeof(EXT3_INODE_T));
    
    char* path_copy = kmalloc(strlen(path) + 1);
    if (!path_copy) { kfree(file); return NULL; }
    strcpy(path_copy, path);
    file->path = path_copy;
    
    return file;
}

uint32_t ext3_read(EXT3_FILE_T* file, uint8_t* buffer, uint32_t size)
{
    if (!file || !buffer || !ext3_initialized) return 0;
    if (file->position >= file->size) return 0;
    
    uint32_t remaining = file->size - file->position;
    uint32_t to_read = (size < remaining) ? size : remaining;
    uint32_t total_read = 0;
    
    uint32_t block_index = file->position / block_size;
    uint32_t offset_in_block = file->position % block_size;
    
    while (total_read < to_read) {
        uint32_t block = block_from_inode(&file->inode, block_index);
        if (!block) break;
        
        uint32_t chunk = block_size - offset_in_block;
        if (chunk > to_read - total_read)
            chunk = to_read - total_read;
        
        uint8_t* buf = kmalloc(block_size);
        if (!buf) break;
        read_block(block, buf);
        memcpy(buffer + total_read, buf + offset_in_block, chunk);
        kfree(buf);
        
        total_read += chunk;
        offset_in_block = 0;
        block_index++;
    }
    
    file->position += total_read;
    return total_read;
}

uint32_t ext3_write(EXT3_FILE_T* file, const uint8_t* buffer, uint32_t size)
{
    if (!file || !buffer || !ext3_initialized) return 0;
    
    if (file->flags & 0x01) {
        uint32_t written = ext3_write_file(file->path, buffer, size);
        if (written) {
            file->size = written;
            file->position = written;
            find_file(file->path, &file->inode, &file->inode_num);
        }
        return written;
    }
    
    return 0;
}

void ext3_close(EXT3_FILE_T* file)
{
    if (!file) return;
    if (file->path) kfree(file->path);
    kfree(file);
}

uint32_t ext3_seek(EXT3_FILE_T* file, uint32_t offset, uint8_t whence)
{
    if (!file) return 0;
    
    switch(whence) {
        case 0:
            file->position = offset;
            break;
        case 1:
            file->position += offset;
            break;
        case 2:
            file->position = file->size - offset;
            break;
    }
    
    if (file->position > file->size)
        file->position = file->size;
    
    return file->position;
}

uint32_t ext3_tell(EXT3_FILE_T* file)
{
    if (!file) return 0;
    return file->position;
}

bool ext3_eof(EXT3_FILE_T* file)
{
    if (!file) return true;
    return file->position >= file->size;
}

EXT3_DIR_T* ext3_opendir(const char* path)
{
    if (!ext3_initialized || !path) return NULL;
    
    EXT3_INODE_T inode;
    uint32_t inode_num;
    if (!find_file(path, &inode, &inode_num)) return NULL;
    if (!(inode.mode & 0x4000)) return NULL;
    
    EXT3_DIR_T* dir = kmalloc(sizeof(EXT3_DIR_T));
    if (!dir) return NULL;
    
    dir->inode_num = inode_num;
    dir->position = 0;
    dir->size = inode.size;
    memcpy(&dir->inode, &inode, sizeof(EXT3_INODE_T));
    dir->current_offset = 0;
    dir->buffer = kmalloc(inode.size + block_size);
    if (!dir->buffer) { kfree(dir); return NULL; }
    memset(dir->buffer, 0, inode.size + block_size);
    
    for (uint32_t i = 0; i < 12; i++) {
        if (inode.block[i] == 0) break;
        read_block(inode.block[i], dir->buffer + (i * block_size));
    }
    
    return dir;
}

const char* ext3_readdir(EXT3_DIR_T* dir)
{
    if (!dir || !ext3_initialized) return NULL;
    if (dir->current_offset >= dir->size) return NULL;
    
    static char name_buffer[256];
    
    while (dir->current_offset < dir->size) {
        EXT3_DIR_ENTRY_T* entry = (EXT3_DIR_ENTRY_T*)(dir->buffer + dir->current_offset);
        if (entry->inode == 0) {
            dir->current_offset = dir->size;
            return NULL;
        }
        
        uint32_t name_len = entry->name_len;
        if (name_len > 255) name_len = 255;
        memcpy(name_buffer, entry->name, name_len);
        name_buffer[name_len] = '\0';
        
        dir->current_offset += entry->rec_len;
        
        if (name_buffer[0] == '.' && (name_len == 1 || (name_len == 2 && name_buffer[1] == '.'))) {
            continue;
        }
        
        return name_buffer;
    }
    
    return NULL;
}

void ext3_closedir(EXT3_DIR_T* dir)
{
    if (!dir) return;
    if (dir->buffer) kfree(dir->buffer);
    kfree(dir);
}