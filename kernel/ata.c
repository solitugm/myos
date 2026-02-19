#include <stdint.h>
#include "ata.h"
#include "port.h"

#define ATA_IO_BASE   0x1F0
#define ATA_CTRL_BASE 0x3F6

#define ATA_REG_DATA      (ATA_IO_BASE + 0)
#define ATA_REG_ERROR     (ATA_IO_BASE + 1)
#define ATA_REG_SECCOUNT0 (ATA_IO_BASE + 2)
#define ATA_REG_LBA0      (ATA_IO_BASE + 3)
#define ATA_REG_LBA1      (ATA_IO_BASE + 4)
#define ATA_REG_LBA2      (ATA_IO_BASE + 5)
#define ATA_REG_HDDEVSEL  (ATA_IO_BASE + 6)
#define ATA_REG_COMMAND   (ATA_IO_BASE + 7)
#define ATA_REG_STATUS    (ATA_IO_BASE + 7)

#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_BSY  0x80

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_PIO 0x20

static int ata_ready = 0;

static int ata_wait_not_busy(void) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (!(st & ATA_SR_BSY)) return 0;
    }
    return ATA_ERR_TIMEOUT;
}

static int ata_wait_drq(void) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (st & ATA_SR_DF) return ATA_ERR_DF;
        if (st & ATA_SR_ERR) return ATA_ERR_ABRT;
        if (st & ATA_SR_DRQ) return 0;
    }
    return ATA_ERR_TIMEOUT;
}

int ata_init(void) {
    outb(ATA_CTRL_BASE, 0x02);

    outb(ATA_REG_HDDEVSEL, 0xA0);
    io_wait();

    outb(ATA_REG_SECCOUNT0, 0);
    outb(ATA_REG_LBA0, 0);
    outb(ATA_REG_LBA1, 0);
    outb(ATA_REG_LBA2, 0);
    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t st = inb(ATA_REG_STATUS);
    if (st == 0) {
        ata_ready = 0;
        return ATA_ERR_NO_DRIVE;
    }

    if (ata_wait_not_busy() < 0) {
        ata_ready = 0;
        return ATA_ERR_TIMEOUT;
    }

    uint8_t lba1 = inb(ATA_REG_LBA1);
    uint8_t lba2 = inb(ATA_REG_LBA2);
    if (lba1 != 0 || lba2 != 0) {
        ata_ready = 0;
        return ATA_ERR_NO_DRIVE;
    }

    int drq = ata_wait_drq();
    if (drq < 0) {
        ata_ready = 0;
        return drq;
    }

    for (int i = 0; i < 256; i++) {
        (void)inw(ATA_REG_DATA);
    }

    ata_ready = 1;
    return 0;
}

int ata_read28(uint32_t lba, uint8_t count, void* buf) {
    if (!ata_ready) return ATA_ERR_NO_DRIVE;
    if (count == 0) return 0;
    if ((lba >> 28) != 0) return ATA_ERR_IO;

    uint16_t* dst = (uint16_t*)buf;

    for (uint8_t s = 0; s < count; s++) {
        uint32_t cur = lba + s;

        if (ata_wait_not_busy() < 0) return ATA_ERR_TIMEOUT;

        outb(ATA_REG_HDDEVSEL, 0xE0 | ((cur >> 24) & 0x0F));
        outb(ATA_REG_SECCOUNT0, 1);
        outb(ATA_REG_LBA0, (uint8_t)(cur & 0xFF));
        outb(ATA_REG_LBA1, (uint8_t)((cur >> 8) & 0xFF));
        outb(ATA_REG_LBA2, (uint8_t)((cur >> 16) & 0xFF));
        outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        int rc = ata_wait_drq();
        if (rc < 0) return rc;

        for (int i = 0; i < 256; i++) {
            *dst++ = inw(ATA_REG_DATA);
        }
    }

    return 0;
}
