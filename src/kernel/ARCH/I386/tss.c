#include "tss.h"

static tss_entry_t tss_entry;

void write_tss(uint32_t num, uint16_t ss0, uint32_t esp0){
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    memset(&tss_entry, 0, sizeof(tss_entry_t));

    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;

    tss_entry.iomap_base = sizeof(tss_entry_t);

    tss_entry.cs = 0x1B;
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x23;

    set_gdt_entry(num, base, limit, 0x89, 0);
}