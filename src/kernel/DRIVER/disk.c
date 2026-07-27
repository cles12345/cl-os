#include "disk.h"

static inline int wait_for_disk(void){
    uint32_t timeouts = 0;

    while ((inb(ATA_STATUS_PORT)) & 0x80){
        if (timeouts++ > 100000) return 0;
    }

    if (inb(ATA_STATUS_PORT) & 0x01){
        return 0;
    }

    return 1;
}

void read_sectors(uint32_t lba, uint32_t sector_count, void* buffer){
    if(!wait_for_disk()) return;
    
    uint8_t *buffer2 = buffer;
    uint32_t buffer_index = 0;
    
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT_PORT, sector_count);
    outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    outb(ATA_STATUS_PORT, 0x20);
    
    for (uint32_t i = 0; i < sector_count; i++){
        uint32_t timeouts = 0;
        while (!(inb(ATA_STATUS_PORT) & 0x08)){
            if (timeouts++ > 100000) return;
            if (inb(ATA_STATUS_PORT) & 0x01) return;
        }
        for (int x = 0; x < 256; x++){
            uint16_t word = inw(ATA_DATA_PORT);
            buffer2[buffer_index++] = (uint8_t)word;
            buffer2[buffer_index++] = (uint8_t)(word >> 8);
        }
        io_wait();
    }
}

void write_sectors(uint32_t lba, uint32_t sector_count, void* buffer){
    if(!wait_for_disk()) return;
    
    uint8_t *buffer2 = buffer;
    uint32_t buffer_index = 0;
    
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT_PORT, sector_count);
    outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    outb(ATA_STATUS_PORT, 0x30);

    for (uint32_t i = 0; i < sector_count; i++){
         uint32_t timeouts = 0;
        while (!(inb(ATA_STATUS_PORT) & 0x08)){
            if (timeouts++ > 100000) return;
            if (inb(ATA_STATUS_PORT) & 0x01) return;
        }    
        for (int x = 0; x < 256; x++){
            uint16_t low = buffer2[buffer_index++];
            uint16_t high = buffer2[buffer_index++];
            uint16_t word = low | (high << 8);
            outw(ATA_DATA_PORT, word);
            io_wait();
        }
    }
}
