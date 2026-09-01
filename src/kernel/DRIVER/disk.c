#include "disk.h"

static inline int wait_for_disk(void){
    uint32_t timeouts = 0;

    while ((inb(ATA_STATUS_PORT)) & ATA_SR_BSY){
        if (timeouts++ > 100000) return 0;
    }

    if (inb(ATA_STATUS_PORT) & ATA_SR_ERR){
        return 0;
    }

    return 1;
}

void read_sectors(uint32_t lba, uint32_t sector_count, void* buffer){
    if(!wait_for_disk()) return;
    
    uint16_t *buffer2 = buffer;
    uint32_t buffer_index = 0;
    
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    outb(ATA_SECTOR_COUNT_PORT, sector_count);
    outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND_PORT, ATA_CMD_READ_PIO);
    
    for (uint32_t i = 0; i < sector_count; i++){
        uint32_t timeouts = 0;
        while (!(inb(ATA_STATUS_PORT) & ATA_SR_DRQ)){
            if (timeouts++ > 100000) return;
            if (inb(ATA_STATUS_PORT) & ATA_SR_ERR) return;
        }
        for (int x = 0; x < 256; x++){
            buffer2[buffer_index++] = inw(ATA_DATA_PORT);
        }
    }
}

void write_sectors(uint32_t lba, uint32_t sector_count, void* buffer){
    if (sector_count == 0) return;
    if(!wait_for_disk()) return;
    
    uint16_t *buffer2 = buffer;
    uint32_t buffer_index = 0;
    
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    outb(ATA_SECTOR_COUNT_PORT, sector_count);
    outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_PIO);

    for (uint32_t i = 0; i < sector_count; i++){
         uint32_t timeouts = 0;
        while (!(inb(ATA_STATUS_PORT) & ATA_SR_DRQ)){
            if (inb(ATA_STATUS_PORT) & ATA_SR_ERR) return;
            if (timeouts++ > 100000) return;
        }    
        for (int x = 0; x < 256; x++){
            outw(ATA_DATA_PORT, buffer2[buffer_index++]);
        }
    }

    outb(ATA_COMMAND_PORT, ATA_CMD_CACHE_FLUSH);
    wait_for_disk();
}
