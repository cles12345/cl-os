#include "syscall.h"

void sys_write(unsigned int fd, const char* str, uint32_t count){
    for (uint32_t i = 0; i < count; i++){
        printc(str[i]);
    }
}

void syscall_dispatcher(intrupt_registers_t* regs){
    switch (regs->eax){
        case 4: sys_write((const char*)regs->ebx, regs->ecx, regs->edx);break;
        default: print("unknown syscall");
    }
}