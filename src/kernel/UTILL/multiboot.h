#pragma once
#include "stdint.h"

typedef struct{
    uint32_t table_size;
    uint32_t str_size;
    uint32_t addr;
    uint32_t reserved;
} multiboot_aout_symbol_table_t;

typedef struct{
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
} multiboot_elf_section_header_t;

typedef struct{
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_uper;
    uint32_t boot_device;

    uint32_t cmd_line;
    uint32_t mods_count;
    uint32_t mods_addr;

    union {
        multiboot_aout_symbol_table_t aout_symbol;
        multiboot_elf_section_header_t elf_section;
    } u;
    
    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;
    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} multiboot_info_t;

typedef struct{
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4
#define MULTIBOOT_MEMORY_BADRAM 5
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;