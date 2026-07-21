#pragma once
#include "stdint.h"
#include "DRIVER/vga.h"

void *memset(void* dest, char val, uint32_t count);
void kernel_panic(void);
void outb(uint16_t port, uint8_t value);

typedef struct{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t intrupt_number, error_code; 
    uint32_t eip, csm, eflags, useresp, ss;
} intrupt_registers_t;