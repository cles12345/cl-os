#pragma once

#include "MM/heap.h"
#include "FS/vfs.h"
#include "elf.h"
#include "stdint.h"

#define PROC_READY    0
#define PROC_RUNNING  1
#define PROC_BLOCKED  2
#define PROC_TERMINATED 3
#define PROC_IDLE 4

#define MAX_PROCESSES 4194304
#define PROC_NAME_MAX 32

typedef struct process {
    uint32_t pid;
    uint32_t state;
    char name[PROC_NAME_MAX];

    uint32_t* page_dir;
    fd_table_t* fd_table;
    uint32_t entry_point;

    intrupt_registers_t regs;

    struct process* next;
    struct process* prev;
} process_t;

void init_process(void);
process_t* create_process(const char* path);
intrupt_registers_t* schedule(intrupt_registers_t* regs);
fd_table_t* get_fd(uint32_t pid);