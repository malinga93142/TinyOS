
#pragma once
#include <stddef.h>
#include <stdint.h>

#define ATA_SECTOR_SIZE 512

enum {
  ATA_OK = 0,
  ATA_ERR_NODEV = -1,
  ATA_ERR_NOTATA = -2,
  ATA_ERR_TIMEOUT = -3,
  ATA_ERR_DEVICE = -4,
  ATA_ERR_RANGE = -5,
};

typedef struct {
  uint16_t io;
  uint16_t ctrl;
  uint8_t slave;
  uint8_t present;
  uint8_t lba48;
  uint64_t sectors;
  char model[41];
} ata_drive_t;

extern ata_drive_t ata_drives[4];

void ata_init(void);
int ata_read(ata_drive_t *d, uint64_t lba, uint32_t count, void *buf);
int ata_write(ata_drive_t *d, uint64_t lba, uint32_t conunt, const void *buf);
void ata_selftest(void);
