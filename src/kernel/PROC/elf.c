#include "elf.h"

int elf_load(uint8_t* elf_data, uint32_t* entry_point, uint32_t* page_dir){
    elf_header_t* header = (elf_header_t*)elf_data;
    if (validate_elf(header) < 0){
        return -1;
    }

    *entry_point = header->e_entry;

    elf_program_header_t* program_header = (elf_program_header_t*)((uint32_t)header + header->e_phoff);

    uint32_t* old_page_dir = mem_get_current_page_dir();
    mem_change_page_dir(page_dir);

    for (int i = 0; i < header->e_phnum; i++){
        if (program_header[i].p_type == PT_LOAD){
            uint32_t vaddr = program_header[i].p_vaddr;
            uint32_t memsz = program_header[i].p_memsz;
            uint32_t filesz = program_header[i].p_filesz;
            uint32_t offset = program_header[i].p_offset;

            uint32_t num_pages = CEIL_DIV(memsz, 0x1000);
            for (uint32_t x = 0; x < num_pages; x++){
                uint32_t phys = pmm_alloc_page_frame();
                if (phys == 0){
                    print("out of memory");
                    mem_change_page_dir(old_page_dir);
                    return -1;
                }
                mem_map_page(vaddr + x * 0x1000, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER);
            }

            if (filesz > 0){
                memcpy((void*)vaddr, elf_data + offset, filesz);
            }

            if (memsz > filesz){
                memset((void*)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }
    
    uint32_t stack_top = STACK_ELF_START;
    uint32_t stack_size = 0x1000;

    for (uint32_t addr = stack_top - stack_size; addr < stack_top; addr += 0x1000){
        uint32_t phys = pmm_alloc_page_frame();
        if (phys ==  0){
            print("out of memory");
            return -1;
        }
        mem_map_page(addr, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    }
    mem_change_page_dir(old_page_dir);
    return 0;
}

int validate_elf(elf_header_t* header){
    if (header->e_ident[0] != EI_MAG0 || header->e_ident[1] != EI_MAG1 || header->e_ident[2] != EI_MAG2 || header->e_ident[3] != EI_MAG3){
        print("invalid elf magic\n");
        return -1;
    }
    
    if (header->e_ident[EI_CLASS] != ELF_CLASS_32){
        print("not 32-bit ELF\n");
        return -1;
    }

    if (header->e_machine != EM_386){
        print("not x86 ELF\n");
        return -1;
    }

    if (header->e_type != ET_EXEC){
        print("not executable\n");
        return -1;
    }

    return 0;
}