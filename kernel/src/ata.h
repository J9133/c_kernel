#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ata_sector_size 512

int ata_sector_read(uint32_t lba, uint8_t sector_count, uint8_t *buffer);
int ata_sector_write(uint32_t lba, uint8_t sector_count, uint8_t *buffer);

#endif