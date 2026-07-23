#pragma once
#include "stdint.h"
#include "memory.h"
#include "utill.h"

#define HEAP_THRESHOLD 0x10000
#define HEAP_MAGIC 0xDEADBEEF

typedef struct heap_block{
    uint32_t magic; 
    uint32_t size;
    bool used;
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

void kmalloc_init(void);
void change_heap_size(int new_heap_size);
void *kmalloc(uint32_t size);
void kfree(void* ptr);