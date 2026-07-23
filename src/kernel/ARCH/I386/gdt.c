#include "gdt.h"

extern void gdt_flush(gdtr_t* pgdt);
extern void tss_flush(void);
extern uint32_t stack_top;

static gdt_entry_t gdt_entries[6];
static gdtr_t gdtr;

void init_gdt(void){
    gdtr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdtr.base = (uint32_t)gdt_entries;

    set_gdt_entry(0, 0, 0, 0, 0); // null segment
    set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // kernel code segment
    set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // kernel data segment
    set_gdt_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user code segment
    set_gdt_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user data segment
    write_tss(5, 0x10, (uint32_t)&stack_top);

    gdt_flush(&gdtr);
    tss_flush();
}

void set_gdt_entry(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran){
    gdt_entries[num].base_low = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;

    gdt_entries[num].limit = (limit & 0xFFFF);
    
    gdt_entries[num].flags = (limit >> 16) & 0x0F;
    gdt_entries[num].flags |= (gran & 0xF0);

    gdt_entries[num].access = access;
}