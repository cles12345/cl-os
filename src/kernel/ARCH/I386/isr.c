#include "utill.h"
#include "syscall.h"
#include "DRIVER/keyboard.h"

const char* exception_names[32] = {
    "Divide Error",               
    "Debug Exception",            
    "Non-Maskable Interrupt",     
    "Breakpoint",                 
    "Overflow",                   
    "Bound Range Exceeded",       
    "Invalid Opcode",             
    "Device Not Available",       
    "Double Fault",               
    "Coprocessor Segment Overrun",
    "Invalid TSS",                
    "Segment Not Present",        
    "Stack Segment Fault",        
    "General Protection Fault",  
    "Page Fault",                
    "Not an exception",          
    "x87 Floating Point",        
    "Alignment Check",           
    "Machine Check",             
    "SIMD Floating Point",   
    "Virtualization Exception",
    "Not an exception",        
    "Security Exception",      
    ""
};

void isr_handler(intrupt_registers_t* regs){
    if (regs->intrupt_number < 32){
        print("Exception: ");
        print(exception_names[regs->intrupt_number]);
        print("\nError code: ");
        printh(regs->error_code);
        print("\n");
        kernel_panic();
    }
    else if (regs->intrupt_number == 0x80){
        syscall_dispatcher(regs);
    }
    else if(regs->intrupt_number == 33){
        keyboard_handler();
    }
}