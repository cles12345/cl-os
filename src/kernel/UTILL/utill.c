#include "utill.h"

void *memset(void* dest, char val, uint32_t count){
    char *temp = (char*)dest;
    for (; count != 0; count--){
        *temp++ = val;
    }

    return dest;
}

void kernel_panic(void){
	print("KERNEL PANIC!\n");
	while (1)
	{
		asm volatile("cli");
		asm volatile("hlt");
	}
}

void outb(uint16_t port, uint8_t value){
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}