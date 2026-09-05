#pragma once

#include "disk.h"
#include "vga.h"
#include "stdint.h"
#include "MM/heap.h"

typedef struct {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    uint32_t first_inode;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algorithm_usage_bitmap;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t reserved_gdt_blocks;
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t pad1;
    uint16_t desc_size;
    uint32_t default_mount_opts;
    uint32_t first_meta_bg;
    uint32_t mkfs_time;
    uint32_t jnl_blocks[17];
} __attribute__((packed)) EXT3_SUPERBLOCK_T;

typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t osd2[12];
} __attribute__((packed)) EXT3_INODE_T;

typedef struct {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t reserved[12];
} __attribute__((packed)) EXT3_GROUP_DESC_T;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[255];
} __attribute__((packed)) EXT3_DIR_ENTRY_T;

typedef struct {
    uint32_t inode_num;
    uint32_t position;
    uint32_t size;
    uint32_t flags;
    EXT3_INODE_T inode;
    char* path;
} EXT3_FILE_T;

typedef struct {
    uint32_t inode_num;
    uint32_t position;
    uint32_t size;
    uint8_t* buffer;
    EXT3_INODE_T inode;
    uint32_t current_offset;
} EXT3_DIR_T;

void init_ext3(void);

uint32_t ext3_read_file(const char* path, uint8_t* buffer);
uint32_t ext3_write_file(const char* path, const uint8_t* buffer, uint32_t size);
uint32_t ext3_file_size(const char* path);
bool ext3_file_exists(const char* path);

bool ext3_create_directory(const char* path);
bool ext3_list_directory(const char* path);
bool ext3_delete(const char* path);

bool ext3_create_symlink(const char* target, const char* path);
char* ext3_read_symlink(const char* path);
bool ext3_is_symlink(const char* path);

EXT3_FILE_T* ext3_open(const char* path, uint32_t flags);
uint32_t ext3_read(EXT3_FILE_T* file, uint8_t* buffer, uint32_t size);
uint32_t ext3_write(EXT3_FILE_T* file, const uint8_t* buffer, uint32_t size);
void ext3_close(EXT3_FILE_T* file);
uint32_t ext3_seek(EXT3_FILE_T* file, uint32_t offset, uint8_t whence);
uint32_t ext3_tell(EXT3_FILE_T* file);
bool ext3_eof(EXT3_FILE_T* file);

EXT3_DIR_T* ext3_opendir(const char* path);
const char* ext3_readdir(EXT3_DIR_T* dir);
void ext3_closedir(EXT3_DIR_T* dir);