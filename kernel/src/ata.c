#include <stdint.h>
#include "ata.h"
#include "io.h"

// عنوان قاعدة القناة الأساسية (Primary ATA Channel)
#define ATA_PRIMARY_IO_BASE 0x1F0

// ATA_PRIMARY_IO_BASE + إزاحة 
#define ATA_REG_DATA        (ATA_PRIMARY_IO_BASE + 0) // 0x1F0
#define ATA_REG_ERROR       (ATA_PRIMARY_IO_BASE + 1) // 0x1F1
#define ATA_REG_SECCOUNT0   (ATA_PRIMARY_IO_BASE + 2) // 0x1F2
#define ATA_REG_LBA0        (ATA_PRIMARY_IO_BASE + 3) // 0x1F3
#define ATA_REG_LBA1        (ATA_PRIMARY_IO_BASE + 4) // 0x1F4
#define ATA_REG_LBA2        (ATA_PRIMARY_IO_BASE + 5) // 0x1F5
#define ATA_REG_HDDEVSEL    (ATA_PRIMARY_IO_BASE + 6) // 0x1F6
#define ATA_REG_COMMAND     (ATA_PRIMARY_IO_BASE + 7) // 0x1F7
#define ATA_REG_STATUS      (ATA_PRIMARY_IO_BASE + 7) // 0x1F7
#define ATA_CMD_CACHE_FLUSH 0xE7

// أوامر ATA
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30

// بتات بحالة الـ STATUS register
#define ATA_SR_BSY  0x80 // مشغول لسا
#define ATA_SR_ERR  0x01 // خطأ
#define ATA_SR_DRQ  0x08 // جاهز

int ata_sector_write(uint32_t lba, uint8_t sector_count, uint8_t *buffer){
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_REG_SECCOUNT0, sector_count);
    outb(ATA_REG_LBA0, (uint8_t)lba);
    outb(ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    uint64_t buffer_write_buffer = 0;
    for (uint64_t i = 0; i < sector_count; i++){
        for (uint64_t i2 = 0; i2 < ata_sector_size/2; i2++){
            uint16_t word = buffer[buffer_write_buffer] | (buffer[buffer_write_buffer+1] << 8);
            outw(ATA_REG_DATA, word);
            buffer_write_buffer += 2;
        }
    }
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    if ((inb(ATA_REG_STATUS) & ATA_SR_ERR) != 0){
        return inb(ATA_REG_ERROR);
    }
    return 0;
}
int ata_sector_read(uint32_t lba, uint8_t sector_count, uint8_t *buffer){
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_REG_SECCOUNT0, sector_count);
    outb(ATA_REG_LBA0, (uint8_t)lba);
    outb(ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    uint64_t buffer_read_buffer = 0;
    for (uint64_t i = 0; i < sector_count; i++){
        for (uint64_t i2 = 0; i2 < ata_sector_size/2; i2++){
            uint16_t word = inw(ATA_REG_DATA);
            buffer[buffer_read_buffer] = (uint8_t)word;
            buffer[buffer_read_buffer+1] = (uint8_t)word << 8;
            buffer_read_buffer += 2;
        }
    }
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    while((inb(ATA_REG_STATUS) & ATA_SR_BSY) != 0){}
    while((inb(ATA_REG_STATUS) & ATA_SR_DRQ) == 0){}
    if ((inb(ATA_REG_STATUS) & ATA_SR_ERR) != 0){
        return inb(ATA_REG_ERROR);
    }
    return 0;
}