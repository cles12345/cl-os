#include "kernel.h"

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
    int *foo = kmalloc(sizeof(char) * 12);
    *foo = 5;
    print("value: ");
    printi(*foo);
    print("\naddr: ");
    printh((uint32_t)foo);
    print("\n");
    asm volatile("int $0x80");
    print("\n");
    kfree(foo);
    while (1) asm volatile("hlt");
}