#include "kernel.h"

void kmain(void){
    vga_reset();
    print("hello world\ttab\nnew line");
    while (1) asm("hlt");
}