#include "kernel.h"

extern void jump_to_user_mode(uint32_t, uint32_t);

void kmain(uint32_t magic, multiboot_info_t* boot_info){\
    vga_reset();
    enable_cursor(0x0C, 0x0F);
    if (magic != 0x2BADB002){
        print("magic is not equal 0x1BADB002");
        kernel_panic();
    }
    init_gdt();
    init_idt();
    uint32_t mem_high = 0;
    uint32_t physical_alloc_start = 0x00100000;  
    if (boot_info->flags & (1 << 6)){
        multiboot_mmap_entry_t* mmap = (multiboot_mmap_entry_t*)boot_info->mmap_addr;
        multiboot_mmap_entry_t* end = (multiboot_mmap_entry_t*)(boot_info->mmap_addr + boot_info->mmap_length);
        
        while (mmap < end){
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE){
                uint32_t start = mmap->addr_low;
                uint32_t length = mmap->len_low;
                uint32_t end_addr = start + length;
                
                if (end_addr > mem_high) mem_high = end_addr;
                
                if (start >= 0x00100000 && start < physical_alloc_start){
                    physical_alloc_start = start;
                }
            }
            mmap = (multiboot_mmap_entry_t*)((uint32_t)mmap + mmap->size + 4);
        }
    }
    init_memory(mem_high, physical_alloc_start);
    kmalloc_init();
    uint32_t boot_info_page = (uint32_t)boot_info & ~0xFFF;
    mem_map_page(boot_info_page, boot_info_page, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);
    if (boot_info->mods_count > 0){
        uint32_t mod_start = *(uint32_t*)(boot_info->mods_addr);
        uint32_t mod_end = *(uint32_t*)(boot_info->mods_addr + 4);
        uint32_t elf_size = mod_end - mod_start;
        
        uint32_t elf_virt = mod_start + KERNEL_START;
        uint32_t num_pages = CEIL_DIV(elf_size, 0x1000);
        for (uint32_t i = 0; i < num_pages; i++){
            mem_map_page(elf_virt + i * 0x1000, mod_start + i * 0x1000, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);
        }
        
        uint8_t* elf_data = (uint8_t*)elf_virt;

        uint32_t* new_pd = vmm_new_page_dir();
        uint32_t entry_point;
        
        if (elf_load(elf_data, &entry_point, new_pd) == 0){
            for (uint32_t i = 0; i < num_pages; i++){
                mem_unmap_page(elf_virt + i * 0x1000);
            }
            mem_change_page_dir(new_pd);
            jump_to_user_mode(entry_point, STACK_ELF_START);
        }
        else{
            print("couldnt start the ELF\n");
            kernel_panic();
        }
    }
    while (1) asm volatile("hlt");
}