#include "../include/ata.h"
#include "../include/misc.h"
#include "../include/printk.h"

static inline void insw(uint16_t p, void *addr, size_t cnt) {
  __asm__ volatile("rep insw" : "+D"(addr), "+c"(cnt) : "d"(p) : "memory");
}
static inline void outsw(uint16_t p, const void *addr, size_t cnt) {
  __asm__ volatile("rep outsw" : "+S"(addr), "+c"(cnt) : "d"(p) : "memory");
}

#define REG_DATA 0
#define REG_ERROR 1
#define REG_FEATURES 1
#define REG_SECCOUNT 2
#define REG_LBA_LO 3
#define REG_LBA_MID 4
#define REG_LBA_HI 5
#define REG_DEVSEL 6
#define REG_STATUS 7
#define REG_COMMAND 7

#define SR_ERR 0x01
#define SR_DRQ 0x08
#define SR_DF 0x20
#define SR_RDY 0x40
#define SR_BSY 0x80

#define CTL_NIEN 0x02
#define CTL_SRST 0x04

#define CMD_READ_PIO 0x20
#define CMD_READ_PIO_EXT 0x24
#define CMD_WRITE_PIO 0x30
#define CMD_WRITE_PIO_EXT 0x34
#define CMD_FLUSH 0xE7
#define CMD_FLUSH_EXT 0xEA
#define CMD_IDENTIFY 0xEC

#define POLL_TRIES 1000000u

ata_drive_t ata_drives[4];

static inline void delay400(const ata_drive_t *d) {
  inb(d->ctrl);
  inb(d->ctrl);
  inb(d->ctrl);
  inb(d->ctrl);
}
static int wait_not_busy(const ata_drive_t *d) {
  for (unsigned i = 0; i < POLL_TRIES; i++) {
    if (!(inb(d->ctrl) & SR_BSY))
      return ATA_OK;
  }
  return ATA_ERR_TIMEOUT;
}

static int wait_drq(const ata_drive_t *d) {
  delay400(d);
  for (unsigned i = 0; i < POLL_TRIES; i++) {
    uint8_t s = inb(d->ctrl);
    if (s & SR_BSY)
      continue;
    if (s & (SR_ERR | SR_DF))
      return ATA_ERR_DEVICE;
    if (s & SR_DRQ)
      return ATA_OK;
  }
  return ATA_ERR_TIMEOUT;
}

static void select_drive(const ata_drive_t *d, uint8_t extra) {
  outb(d->io + REG_DEVSEL, (uint8_t)(extra | (d->slave << 4)));
  delay400(d);
}

static void setup_xfer(const ata_drive_t *d, uint64_t lba, uint32_t n,
                       int ext) {
  uint16_t io = d->io;
  if (ext) {
    select_drive(d, 0xE0);
    outb(io + REG_SECCOUNT, (uint8_t)(n >> 8));
    outb(io + REG_LBA_LO, (uint8_t)(lba >> 24));
    outb(io + REG_LBA_MID, (uint8_t)(lba >> 32));
    outb(io + REG_LBA_HI, (uint8_t)(lba >> 40));
    outb(io + REG_SECCOUNT, (uint8_t)n);
    outb(io + REG_LBA_LO, (uint8_t)lba);
    outb(io + REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(io + REG_LBA_HI, (uint8_t)(lba >> 16));
  } else {
    select_drive(d, 0xE0 | ((lba >> 24) & 0x0F)); /* LBA bit + LBA[27:24] */
    outb(io + REG_SECCOUNT, (uint8_t)n);
    outb(io + REG_LBA_LO, (uint8_t)lba);
    outb(io + REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(io + REG_LBA_HI, (uint8_t)(lba >> 16));
  }
}

static int identify(ata_drive_t *d) {
  uint16_t id[256];
  uint16_t io = d->io;

  d->present = 0;

  if (inb(d->ctrl) == 0xFF)
    return ATA_ERR_NODEV;

  outb(d->ctrl, CTL_NIEN);
  select_drive(d, 0xa0);
  outb(io + REG_SECCOUNT, 0);
  outb(io + REG_LBA_LO, 0);
  outb(io + REG_LBA_MID, 0);
  outb(io + REG_LBA_HI, 0);
  outb(io + REG_COMMAND, CMD_IDENTIFY);
  delay400(d);

  if (inb(d->ctrl) == 0)
    return ATA_ERR_NODEV;
  int rc = wait_not_busy(d);
  if (rc)
    return rc;

  if (inb(io + REG_LBA_MID) || inb(io + REG_LBA_HI))
    return ATA_ERR_NOTATA;
  rc = wait_drq(d);
  if (rc)
    return rc;
  insw(io + REG_DATA, id, 256);

  for (int i = 0; i < 20; i++) {
    d->model[2 * i] = (char)(id[27 + i] >> 8);
    d->model[2 * i + 1] = (char)(id[27 + i] & 0xff);
  }
  d->model[40] = 0;

  for (int i = 39; i >= 0 && d->model[i] == ' '; i--)
    d->model[i] = 0;

  d->lba48 = (id[83] >> 10) & 1;
  if (d->lba48)
    d->sectors = (uint64_t)id[100] | (uint64_t)id[101] << 16 |
                 (uint64_t)id[102] << 32 | (uint64_t)id[103] << 48;
  d->present = 1;
  return ATA_OK;
}

void ata_init(void) {
  static const struct {
    uint16_t io, ctrl;
  } bus[2] = {{0x1f0, 0x3f6}, {0x170, 0x376}};

  for (int i = 0; i < 4; i++) {
    ata_drives[i] = (ata_drive_t){
        .io = bus[i >> 1].io, .ctrl = bus[i >> 1].ctrl, .slave = i & 1};
    identify(&ata_drives[i]);
  }
}

static int check_args(const ata_drive_t *d, uint64_t lba, uint32_t count) {
  if (!d || !d->present)
    return ATA_ERR_NODEV;
  if (lba >= d->sectors || count > d->sectors - lba)
    return ATA_ERR_RANGE;
  if (!d->lba48 && lba + count > 0x10000000ull)
    return ATA_ERR_RANGE;
  return ATA_OK;
}

int ata_read(ata_drive_t *d, uint64_t lba, uint32_t count, void *buf) {
  int rc = check_args(d, lba, count);

  if (rc)
    return rc;
  uint8_t *p = buf;
  while (count) {
    uint32_t n = count > 256 ? 256 : count;
    if ((rc = wait_not_busy(d)))
      return rc;
    setup_xfer(d, lba, n, d->lba48);
    outb(d->io + REG_COMMAND, d->lba48 ? CMD_READ_PIO_EXT : CMD_READ_PIO);
    for (uint32_t i = 0; i < n; i++) {
      if ((rc = wait_drq(d)))
        return rc;
      insw(d->io + REG_DATA, p, 256);
      p += ATA_SECTOR_SIZE;
    }
    lba += n;
    count -= n;
  }
  return ATA_OK;
}

int ata_write(ata_drive_t *d, uint64_t lba, uint32_t count, const void *buf) {
  int rc = check_args(d, lba, count);
  if (rc)
    return rc;

  const uint8_t *p = buf;
  while (count) {
    uint32_t n = count > 256 ? 256 : count;
    if ((rc = wait_not_busy(d)))
      return rc;
    setup_xfer(d, lba, n, d->lba48);
    outb(d->io + REG_COMMAND, d->lba48 ? CMD_WRITE_PIO_EXT : CMD_WRITE_PIO);
    for (uint32_t i = 0; i < n; i++) {
      if ((rc = wait_drq(d)))
        return rc;
      outsw(d->io + REG_DATA, p, 256);
      p += ATA_SECTOR_SIZE;
    }
    lba += n;
    count -= n;
  }

  delay400(d);
  if ((rc = wait_not_busy(d)))
    return rc;
  if (inb(d->ctrl) & (SR_ERR | SR_DF)) {
    kprintf("ata_write: status=%x error=%x\n", inb(d->ctrl),
            inb(d->io + REG_ERROR));
    return ATA_ERR_DEVICE;
  }
  outb(d->io + REG_COMMAND, d->lba48 ? CMD_FLUSH_EXT : CMD_FLUSH);
  delay400(d);
  if ((rc = wait_not_busy(d)))
    return rc;
  {
    uint8_t s = inb(d->ctrl);
    if (s & (SR_ERR | SR_DF)) {
      kprintf("ata flush: status=%x err=%x\n", s, inb(d->io + REG_ERROR));
      return ATA_ERR_DEVICE;
    }
  }
  return ATA_OK;
}

static uint8_t wbuf[512 * 4] __attribute__((aligned(16)));
static uint8_t rbuf[512 * 4] __attribute__((aligned(16)));
void ata_selftest(void) {
  ata_drive_t *d = &ata_drives[0];
  for (int i = 0; i < (int)sizeof wbuf; i++) {
    wbuf[i] = (uint8_t)(i * 7 + 3);
  }
  int rc = ata_write(d, 100, 4, wbuf);
  kprintf("write rc=%d\n", rc);
  for (int i = 0; i < (int)sizeof rbuf; i++) {
    rbuf[i] = 0;
  }
  rc = ata_read(d, 100, 4, rbuf);
  kprintf("read rc=%d\n", rc);
  int bad = 0;
  for (int i = 0; i < (int)sizeof wbuf; i++) {
    if (rbuf[i] != wbuf[i])
      bad++;
  }
  kprintf("mismatches: %d\n", bad);
  kprintf("oob rc=%d (expect %d)\n", ata_read(d, 40960, 1, rbuf),
          ATA_ERR_RANGE);
}

void ata_write_hello(void) {
  static uint8_t sec[ATA_SECTOR_SIZE] __attribute__((aligned(16)));
  static uint8_t chk[ATA_SECTOR_SIZE] __attribute__((aligned(16)));

  for (int i = 0; i < ATA_SECTOR_SIZE; i++)
    sec[i] = 0;
  const char msg[] = "hello";
  for (int i = 0; i < 5; i++)
    sec[i] = (uint8_t)msg[i];

  int rc = ata_write(&ata_drives[0], 0, 1, sec);
  kprintf("write rc=%d\n", rc);

  rc = ata_read(&ata_drives[0], 0, 1, chk);
  kprintf("read rc=%d, data=%s\n", rc, (char *)chk);
}
