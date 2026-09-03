#include "kernel.h"

extern void jump_to_user_mode(uint32_t, uint32_t);
extern uint32_t kernel_start;
extern uint32_t kernel_end;

void kmain(uint32_t magic, multiboot_info_t* boot_info){
    vga_reset();
    enable_cursor(0x0C, 0x0F);
    if (magic != 0x2BADB002){
        print("magic is not equal 0x1BADB002");
        kernel_panic();
    }
    init_gdt();
    init_idt();
    uint32_t kernel_end_phys = (uint32_t)&kernel_end - KERNEL_START;
    uint32_t physical_alloc_start = (kernel_end_phys + 0x1000) & ~0xFFF;
    uint32_t mem_high = 0;
    if (boot_info->flags & (1 << 6)){
        multiboot_mmap_entry_t* mmap = (multiboot_mmap_entry_t*)boot_info->mmap_addr;
        multiboot_mmap_entry_t* end = (multiboot_mmap_entry_t*)(boot_info->mmap_addr + boot_info->mmap_length);
        
        while (mmap < end){
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE){
                uint32_t end_addr = mmap->addr_low + mmap->len_low;
                if (end_addr > mem_high) mem_high = end_addr;
            }
            mmap = (multiboot_mmap_entry_t*)((uint32_t)mmap + mmap->size + 4);
        }
    }
    if (mem_high == 0){
        mem_high = 0x10000000; 
    }
    init_memory(mem_high, physical_alloc_start);
    kmalloc_init();
    init_ext3();
    
    if (!ext3_file_exists("/elf")) {
        print("ELF file not found\n");
        kernel_panic();
    }
    uint32_t file_size = ext3_file_size("/elf");

    uint8_t *elf_file = kmalloc(file_size);
    if (elf_file == 0) {
        print("Out of memory\n");
        kernel_panic();
    }
    uint32_t bytes_read = ext3_read_file("/elf", elf_file);
    if (bytes_read != file_size) {
        print("Failed to read ELF file\n");
        kfree(elf_file);
        kernel_panic();
    }

    uint32_t* new_pd = vmm_new_page_dir();
    uint32_t entry_point;

    if (elf_load(elf_file, &entry_point, new_pd) == 0){
        kfree(elf_file);
        mem_change_page_dir(new_pd);
        jump_to_user_mode(entry_point, STACK_ELF_START);
    }
    else{
        print("couldnt start the ELF\n");
        kfree(elf_file);
        kernel_panic();
    }
    while (1) asm volatile("hlt");
}