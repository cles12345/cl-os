#include "kernel.h"

void kmain(void){
    init_gdt();
    vga_reset();
    print("Hello world!");
    while (1) asm volatile("hlt");
}