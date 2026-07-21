#include "kernel.h"

void kmain(void){
    init_gdt();
    init_idt();
    vga_reset();
    print("Hello world!\n");
    print("Dividing by zero...\n");
    asm volatile("mov $0, %ecx; div %ecx");
    while (1) asm volatile("hlt");
}