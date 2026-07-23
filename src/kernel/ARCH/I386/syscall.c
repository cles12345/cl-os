#include "syscall.h"

void syscall_dispatcher(intrupt_registers_t* regs){
    switch (regs->eax){
        default: print("unknown syscall");
    }
}