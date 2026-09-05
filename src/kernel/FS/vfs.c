#include "vfs.h"

static vfs_ops_t* vfs_current_fs = NULL;
fd_table_t* vfs_fd_table = NULL;

static vfs_ops_t vfs_fat32_ops = {
    .exists = fat32_file_exists,
    .size = fat32_file_size,
    .read = fat32_read_file,
    .write = fat32_write_file,
    .mkdir = fat32_create_directory,
    .list = fat32_list_directory,
    .delete = fat32_delete,
    .symlink = NULL,
    .readlink = NULL,
    .is_symlink = NULL,
};

static vfs_ops_t vfs_ext3_ops = {
    .exists = ext3_file_exists,
    .size = ext3_file_size,
    .read = ext3_read_file,
    .write = ext3_write_file,
    .mkdir = ext3_create_directory,
    .list = ext3_list_directory,
    .delete = ext3_delete,
    .symlink = ext3_create_symlink,
    .readlink = ext3_read_symlink,
    .is_symlink = ext3_is_symlink,
};

static void vfs_detect_fs(void)
{
    uint8_t* buf = kmalloc(1024);
    if (!buf) {
        vfs_current_fs = &vfs_fat32_ops;
        init_fat32();
        return;
    }
    
    read_sectors(2, 2, buf);
    uint16_t magic = *(uint16_t*)(buf + 56);
    kfree(buf);
    
    if (magic == 0xEF53) {
        init_ext3();
        vfs_current_fs = &vfs_ext3_ops;
    } else {
        init_fat32();
        vfs_current_fs = &vfs_fat32_ops;
    }
}

void init_vfs(void)
{
    vfs_detect_fs();
    vfs_fd_table = vfs_fd_table_create();
}

fd_table_t* vfs_fd_table_create(void)
{
    fd_table_t* table = kmalloc(sizeof(fd_table_t));
    if (!table) return NULL;
    
    memset(table, 0, sizeof(fd_table_t));
    return table;
}

void vfs_fd_table_destroy(fd_table_t* table)
{
    if (!table) return;
    
    for (int i = 0; i < FD_MAX; i++) {
        if (table->files[i]) {
            table->files[i]->ref_count--;
            if (table->files[i]->ref_count == 0) {
                kfree(table->files[i]);
            }
        }
    }
    
    kfree(table);
}

static int vfs_fd_alloc(fd_table_t* table, vfs_file_t* file)
{
    if (!table || !file) return -1;
    
    for (int i = 3; i < FD_MAX; i++) {
        if (table->files[i] == NULL) {
            table->files[i] = file;
            table->count++;
            file->ref_count++;
            return i;
        }
    }
    
    return -1;
}

static vfs_file_t* vfs_fd_get(fd_table_t* table, int fd)
{
    if (!table || fd < 0 || fd >= FD_MAX) return NULL;
    return table->files[fd];
}

static void vfs_fd_free(fd_table_t* table, int fd)
{
    if (!table || fd < 0 || fd >= FD_MAX) return;
    if (!table->files[fd]) return;
    
    table->files[fd]->ref_count--;
    if (table->files[fd]->ref_count == 0) {
        kfree(table->files[fd]);
    }
    table->files[fd] = NULL;
    table->count--;
}

static vfs_file_t* vfs_file_open(const char* path, uint32_t flags)
{
    if (!vfs_current_fs) return NULL;
    if (!vfs_current_fs->exists(path)) {
        if (!(flags & O_CREAT)) return NULL;
        if (!vfs_current_fs->write(path, NULL, 0, 0)) return NULL;
    }
    
    vfs_file_t* file = kmalloc(sizeof(vfs_file_t));
    if (!file) return NULL;
    
    strcpy(file->path, path);
    file->position = (flags & O_APPEND) ? vfs_current_fs->size(path) : 0;
    file->size = vfs_current_fs->size(path);
    file->flags = flags;
    file->ref_count = 1;
    file->private = NULL;
    
    if (flags & O_TRUNC) {
        vfs_current_fs->write(path, NULL, 0, 0);
        file->size = 0;
        file->position = 0;
    }
    
    return file;
}

static uint32_t vfs_file_read(vfs_file_t* file, uint8_t* buffer, uint32_t size)
{
    if (!file || !buffer) return 0;
    if (file->position >= file->size) return 0;
    if (!vfs_current_fs || !vfs_current_fs->read) return 0;
    
    uint32_t remaining = file->size - file->position;
    uint32_t to_read = (size < remaining) ? size : remaining;
    
    uint32_t read = vfs_current_fs->read(file->path, buffer, file->position, to_read);
    if (read > 0) {
        file->position += read;
    }
    
    return read;
}

static uint32_t vfs_file_write(vfs_file_t* file, const uint8_t* buffer, uint32_t size)
{
    if (!file || !buffer) return 0;
    if (!vfs_current_fs || !vfs_current_fs->write) return 0;
    
    if (file->flags & O_APPEND) {
        file->position = file->size;
    }
    
    uint32_t written = vfs_current_fs->write(file->path, buffer, file->position, size);
    if (written > 0) {
        file->position += written;
        if (file->position > file->size) {
            file->size = file->position;
        }
    }
    
    return written;
}

int vfs_open(const char* path, uint32_t flags)
{
    if (!path || !vfs_fd_table) return -1;
    if (!vfs_current_fs) return -1;
    
    vfs_file_t* file = vfs_file_open(path, flags);
    if (!file) return -1;
    
    return vfs_fd_alloc(vfs_fd_table, file);
}

int vfs_close(int fd)
{
    if (!vfs_fd_table) return -1;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
    if (!file) return -1;
    
    vfs_fd_free(vfs_fd_table, fd);
    return 0;
}

uint32_t vfs_read(int fd, uint8_t* buffer, uint32_t size)
{
    if (!vfs_fd_table || !buffer) return 0;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
    if (!file) return 0;
    
    return vfs_file_read(file, buffer, size);
}

uint32_t vfs_write(int fd, const uint8_t* buffer, uint32_t size)
{
    if (!vfs_fd_table || !buffer) return 0;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
    if (!file) return 0;
    
    return vfs_file_write(file, buffer, size);
}

uint32_t vfs_seek(int fd, uint32_t offset, uint8_t whence)
{
    if (!vfs_fd_table) return 0;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
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
    
    if (file->position > file->size) {
        file->position = file->size;
    }
    
    return file->position;
}

uint32_t vfs_tell(int fd)
{
    if (!vfs_fd_table) return 0;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
    if (!file) return 0;
    
    return file->position;
}

uint32_t vfs_size(int fd)
{
    if (!vfs_fd_table) return 0;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
    if (!file) return 0;
    
    return file->size;
}

bool vfs_eof(int fd)
{
    if (!vfs_fd_table) return true;
    
    vfs_file_t* file = vfs_fd_get(vfs_fd_table, fd);
    if (!file) return true;
    
    return file->position >= file->size;
}