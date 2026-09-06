#pragma once
#include "stdint.h"
#include "DRIVER/vga.h"
#include "MM/memory.h"

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_LOPROC 0xFF00
#define ET_HIPROC 0xFFFF

#define EM_NONE 0
#define EM_M32 1
#define EM_SPARC 2
#define EM_386 3
#define EM_68K 4
#define EM_88K 5
#define EM_860 7
#define EM_MIPS 8

#define EV_NONE 0
#define EV_CURRENT 1

#define EI_MAG0 0x7F
#define EI_MAG1 'E'
#define EI_MAG2 'L'
#define EI_MAG3 'F'

#define EI_CLASS 4
#define ELF_CLASS_NONE 0
#define ELF_CLASS_32 1
#define ELF_CLASS_64 2

#define EI_DATA 5
#define ELF_DATA_NONE 0
#define ELF_DATA_2LSB 1
#define ELF_DATA_2MSB 2

#define EI_VERSION 6

#define EI_PAD 7

#define SHN_UNDEF 0
#define SHH_LORESERVE 0xFF00
#define SHH_LOPROC 0xFF00
#define SHH_HIPROC 0xFF1F
#define SHH_ABS 0xFFF1
#define SHH_COMMON 0xFFF2
#define SHH_HIRESERVE 0xFFFF

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTES 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7FFFFFFF
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xFFFFFFFF

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDIR 6
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7FFFFFFF

#define STACK_ELF_START 0xBFFFF000

typedef struct{
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf_header_t;

typedef struct{
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} __attribute__((packed)) elf_section_header_t;

typedef struct{
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf_program_header_t;

int validate_elf(elf_header_t* header);
int elf_load(uint8_t* elf_data, uint32_t* entry_point, uint32_t* page_dir);