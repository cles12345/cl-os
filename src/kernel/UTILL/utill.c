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
	while (1){
		asm volatile("cli");
		asm volatile("hlt");
	}
}

inline void outb(uint16_t port, uint8_t value){
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port){
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

inline void outw(uint16_t port, uint16_t value){
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

inline uint16_t inw(uint16_t port){
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

inline void io_wait(void){
    outb(0x80, 0);
}

void *memcpy(void *dest, void *src, uint32_t count){
    char* d = (char*)dest;
    const char* s = (const char*)src;

    for (uint32_t i = 0; i < count; i++){
        d[i] = s[i];
    }

    return d;
}

bool memcmp(const void* a, const void* b, uint16_t count){
    const uint8_t* tempa = (const uint8_t*)a;
    const uint8_t* tempb = (const uint8_t*)b;

    for (uint32_t i = 0; i < count; i++){
        if (*tempa != *tempb){
            return true;
        }
        tempa++;
        tempb++;
    }
    return false;
}

int strlen(const char* str){
    int i = 0;
    while (*str){
        i++;
        str++;
    }
    return i;
}

char* strcpy(char* dest, const char* src){
    char* original = dest;
    while (*src){
        *dest = *src;
        src++;
        dest++;
    }
    *dest = '\0';
    return original;
}

char* strrchr(const char* str, char c){
    const char* last = 0;
    while (*str) {
        if (*str == c) last = str;
        str++;
    }
    if (c == '\0') return (char*)str;
    return (char*)last;
}

char* strtok(char* str, const char* delim){
    static char* next = 0;
    char* start;
    int i;
    
    if (str != 0)
        start = str;
    else if (next == 0)
        return 0;
    else
        start = next;
    
    while (*start != '\0')
    {
        for (i = 0; delim[i] != '\0'; i++)
            if (*start == delim[i]) break;
        if (delim[i] == '\0') break;
        start++;
    }
    
    if (*start == '\0')
    {
        next = 0;
        return 0;
    }
    
    char* end = start;
    while (*end != '\0')
    {
        for (i = 0; delim[i] != '\0'; i++)
            if (*end == delim[i])
            {
                *end = '\0';
                next = end + 1;
                return start;
            }
        end++;
    }
    
    next = 0;
    return start;
}

char* strchr(const char* str, char c){
    while (*str != '\0')
    {
        if (*str == c)
            return (char*)str;
        str++;
    }
    if (c == '\0')
        return (char*)str;
    return 0;
}

bool strcmp(const char* str1, const char* str2){
    while (*str1 && *str2) {
        if (*str1 != *str2) return true;
        str1++;
        str2++;
    }
    
    if (*str1 == '\0' && *str2 == '\0') return false;
    return true;
}