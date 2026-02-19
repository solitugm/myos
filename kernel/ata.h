#pragma once
#include <stdint.h>

#define ATA_ERR_TIMEOUT  -1
#define ATA_ERR_NO_DRIVE -2
#define ATA_ERR_DF       -3
#define ATA_ERR_ABRT     -4
#define ATA_ERR_IO       -5

int ata_init(void);
int ata_read28(uint32_t lba, uint8_t count, void* buf);
