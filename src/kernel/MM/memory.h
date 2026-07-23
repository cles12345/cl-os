#pragma once
#include "stdint.h"
#include "utill.h"
#include "multiboot.h"
#include "DRIVER/vga.h"

#define KERNEL_START 0xC0000000
#define KERNEL_MALLOC 0xD0000000
#define REC_PAGE_DIR ((uint32_t*)0xFFFFF000)
#define REC_PAGE_TABLE(i) ((uint32_t*)(0xFFC00000 + ((i) * 0x1000)))
#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_USER  (1 << 2)
#define PAGE_FLAG_WRITETHROUGH (1 << 3)
#define PAGE_FLAG_CACHE_DISABLE (1 << 4)
#define PAGE_FLAG_ACCESSED (1 << 5)
#define PAGE_FLAG_DIRTY (1 << 6)
#define PAGE_FLAG_PAT (1 << 7)
#define PAGE_FLAG_GLOBAL (1 << 8)
#define PAGE_FLAG_FRAME (0xFFFFF000)
#define PAGE_FLAG_OWNER (1 << 9)
#define NUM_PAGE_DIRS 256
#define NUM_PAGE_FRAMES (0x10000000 / 0x1000)

void init_memory(uint32_t mem_high, uint32_t physical_alloc_start);
void invalidate(uint32_t vaddr);
void pmm_init(uint32_t mem_low, uint32_t mem_high);
uint32_t pmm_alloc_page_frame(void);
void pmm_free_page_frame(uint32_t addr);
uint32_t* mem_get_current_page_dir(void);
void sync_page_dirs(void);
void mem_map_page(uint32_t virt_addr, uint32_t phy_addr, uint32_t flags);
void mem_unmap_page(uint32_t virt_addr);
uint32_t* vmm_new_page_dir(void);
void vmm_free_page_dir(uint32_t* pd);