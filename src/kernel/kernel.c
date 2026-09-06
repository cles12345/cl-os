#include "kernel.h"

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
    init_fat32();
    init_vfs();
    init_process();

    process_t* process = create_process("/elf");

    if (!process) kernel_panic();

    process_t* process2 = create_process("/elf2");

    if (!process2) kernel_panic();
    while (1) asm volatile("hlt");
}