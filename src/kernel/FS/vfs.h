#pragma once

#include "DRIVER/ext3.h"
#include "DRIVER/fat32.h"

#define FD_MAX 256
#define FD_STDIN 0
#define FD_STDOUT 1
#define FD_STDERR 2

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

typedef struct vfs_file vfs_file_t;
typedef struct fd_table fd_table_t;

struct vfs_file {
    char path[256];
    uint32_t position;
    uint32_t size;
    uint32_t flags;
    uint32_t ref_count;
    void* private;
};

struct fd_table {
    vfs_file_t* files[FD_MAX];
    uint32_t count;
};

typedef struct {
    bool (*exists)(const char* path);
    uint32_t (*size)(const char* path);
    uint32_t (*read)(const char* path, uint8_t* buffer, uint32_t offset, uint32_t size);
    uint32_t (*write)(const char* path, const uint8_t* buffer, uint32_t offset, uint32_t size);
    bool (*mkdir)(const char* path);
    bool (*list)(const char* path);
    bool (*delete)(const char* path);
    bool (*symlink)(const char* target, const char* path);
    char* (*readlink)(const char* path);
    bool (*is_symlink)(const char* path);
} vfs_ops_t;

void init_vfs(void);

fd_table_t* vfs_fd_table_create(void);
void vfs_fd_table_destroy(fd_table_t* table);

int vfs_open(const char* path, uint32_t flags);
int vfs_close(int fd);
uint32_t vfs_read(int fd, uint8_t* buffer, uint32_t size);
uint32_t vfs_write(int fd, const uint8_t* buffer, uint32_t size);
uint32_t vfs_seek(int fd, uint32_t offset, uint8_t whence);
uint32_t vfs_tell(int fd);
uint32_t vfs_size(int fd);
bool vfs_eof(int fd);

extern fd_table_t* vfs_fd_table;