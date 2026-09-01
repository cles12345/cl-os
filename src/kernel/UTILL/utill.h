#pragma once
#include "stdint.h"
#include "DRIVER/vga.h"

#define CEIL_DIV(a, b) (((a + b) - 1)/b)

void *memset(void* dest, char val, uint32_t count);
void kernel_panic(void);
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void io_wait(void);
void *memcpy(void *dest, void *src, uint32_t count);
bool memcmp(const void* a, const void* b, uint16_t count);
int strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strrchr(const char* str, char c);
char* strtok(char* str, const char* delim);
char* strchr(const char* str, char c);
bool strcmp(const char* str1, const char* str2);

typedef struct{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t intrupt_number, error_code; 
    uint32_t eip, csm, eflags, useresp, ss;
} intrupt_registers_t;