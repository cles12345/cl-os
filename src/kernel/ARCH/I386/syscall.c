#include "syscall.h"

void sys_write(const char* str, uint32_t count){
    char temp[2];
    temp[1] = '\0';
    for (uint32_t i = 0; i < count; i++){
        temp[0] = str[i];
        print(temp);
    }
}   

void sys_exit(uint32_t exit_code){
    if (exit_code == 1){
        print("program exited with 1");
    }
    uint32_t* current_page_dir = mem_get_current_page_dir();
    mem_change_page_dir_to_intial();
    vmm_free_page_dir(current_page_dir);
}

void syscall_dispatcher(intrupt_registers_t* regs){
    switch (regs->eax){
        case 0: sys_write((const char*)regs->ebx, regs->ecx);break;
        case 1: sys_exit(regs->ebx);break;
        default: print("unknown syscall");
    }
}