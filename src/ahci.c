#include "../include/ahci.h"
#include "../include/pci.h"
#include "../include/printk.h"
#include "../include/paging.h"
#include "../include/hpet.h"
#include "../include/pmm.h"
#include <stdint.h>

#define PX(pt, off) ((pt)->regs[(off) / 4])
#define CMD_TIMEOUT_US 100000

typedef struct { uint16_t flags, prdtl; uint32_t prdbc, ctba, ctbau, rsvd[4]; } __attribute__((packed)) cmd_hdr_t;
typedef struct {
 uint8_t type, c, command, feat, l[3], dev, h[3], feath;
 uint16_t count;
 uint8_t icc, ctl, rsvd[4];
} __attribute__((packed)) fis_h2d_t;
typedef struct { uint64_t dba; uint32_t rsvd, dbc; } __attribute__((packed)) prd_t;

static ahci_port_t g_ports[32];
static uint32_t g_ready;

static uintptr_t ahci_get_abar(uint8_t b, uint8_t d, uint8_t f) {
 uint64_t pa = pci_config_read(b, d, f, PCI_REG_BAR5) & 0xFFFFFFF0;
 for (int i = 0; i < 2; i++) /* HBA regs span 0x1100 bytes: 2 pages */
  paging_map_page((uint64_t)PA_TO_VA(pa + i * 4096), pa + i * 4096,
                  PAGE_PRESENT | PAGE_PWT | PAGE_PCD | PAGE_WRITE);
 return pa;
}

static volatile uint8_t *alloc_zeroed(uint64_t *pa) {
 *pa = (uint64_t)(uintptr_t)pmm_alloc_frame();
 volatile uint32_t *v = (void *)PA_TO_VA(*pa);
 for (int i = 0; i < 1024; i++)
  v[i] = 0;
 return (volatile uint8_t *)v;
}

static void ahci_port_init(ahci_port_t *pt) {
 uint64_t cl_pa, fis_pa;
 PX(pt, AHCI_PX_CMD) &= ~AHCI_CMD_ST;
 while (PX(pt, AHCI_PX_CMD) & AHCI_CMD_CR)
  ;
 PX(pt, AHCI_PX_CMD) &= ~AHCI_CMD_FRE;
 while (PX(pt, AHCI_PX_CMD) & AHCI_CMD_FR)
  ;
 pt->cl = alloc_zeroed(&cl_pa);
 alloc_zeroed(&fis_pa);
 pt->ct = alloc_zeroed(&pt->ct_pa);
 pt->buf = (volatile uint16_t *)alloc_zeroed(&pt->buf_pa);
 PX(pt, AHCI_PX_CLB) = (uint32_t)cl_pa;
 PX(pt, AHCI_PX_CLBU) = cl_pa >> 32;
 PX(pt, AHCI_PX_FB) = (uint32_t)fis_pa;
 PX(pt, AHCI_PX_FBU) = fis_pa >> 32;
 PX(pt, AHCI_PX_SERR) = 0xFFFFFFFF;
 PX(pt, AHCI_PX_IS) = 0xFFFFFFFF;
 PX(pt, AHCI_PX_CMD) |= AHCI_CMD_FRE;
 PX(pt, AHCI_PX_CMD) |= AHCI_CMD_ST;
}

// Issue one command in slot 0 using the port's single DMA buffer.
static int ahci_cmd(ahci_port_t *pt, uint8_t cmd, uint64_t lba, uint16_t count, uint32_t bytes) {
 cmd_hdr_t *h = (void *)pt->cl;
 fis_h2d_t *f = (void *)pt->ct;
 prd_t *p = (void *)(pt->ct + 0x80);
 *h = (cmd_hdr_t){.flags = 5, .prdtl = 1, .ctba = (uint32_t)pt->ct_pa, .ctbau = pt->ct_pa >> 32};
 *f = (fis_h2d_t){.type = FIS_TYPE_H2D, .c = 0x80, .command = cmd, .dev = 0x40,
                  .l = {lba, lba >> 8, lba >> 16}, .h = {lba >> 24, lba >> 32, lba >> 40},
                  .count = count};
 *p = (prd_t){.dba = pt->buf_pa, .dbc = bytes - 1};
 PX(pt, AHCI_PX_IS) = 0xFFFFFFFF;
 asm volatile("" ::: "memory"); /* descriptors must be written before the doorbell */
 PX(pt, AHCI_PX_CI) = 1;
 uint64_t start = hpet_get_counter(), limit = hpet_us_to_ticks(CMD_TIMEOUT_US);
 while (PX(pt, AHCI_PX_CI) & 1)
  if (hpet_get_counter() - start > limit) {
   kprintf("[AHCI] cmd %x timeout: CI=%x IS=%x TFD=%x\n", cmd,
           PX(pt, AHCI_PX_CI), PX(pt, AHCI_PX_IS), PX(pt, AHCI_PX_TFD));
   return -1;
  }
 if (PX(pt, AHCI_PX_IS) & AHCI_IS_TFES) {
  kprintf("[AHCI] cmd %x: task file error\n", cmd);
  return -1;
 }
 return 0;
}

int ahci_identify(ahci_port_t *pt) {
 if (ahci_cmd(pt, ATA_CMD_IDENTIFY, 0, 0, 512))
  return -1;
 char model[41];
 for (int k = 0; k < 20; k++) { /* ATA strings are byte-swapped per word */
  model[k * 2] = pt->buf[27 + k] >> 8;
  model[k * 2 + 1] = pt->buf[27 + k] & 0xFF;
 }
 int n = 40;
 while (n > 0 && model[n - 1] == ' ')
  n--;
 model[n] = 0;
 kprintf("[AHCI] model = '%s'\n", model);
 kprintf("[AHCI] LBA28 = %u sectors\n", ((uint32_t)pt->buf[61] << 16) | pt->buf[60]);
 return 0;
}

int ahci_read(ahci_port_t *pt, uint64_t lba, uint16_t count) {
 if (count == 0 || count > 8) /* buffer is one 4 KB frame */
  return -1;
 return ahci_cmd(pt, ATA_CMD_READ_DMA_EXT, lba, count, count * 512);
}

void ahci_init(uint8_t bus, uint8_t device, uint8_t func) {
 pci_write_word(bus, device, func, 0x04, pci_read_word(bus, device, func, 0x04) | 0x06);
}

int ahci_probe(uint8_t bus, uint8_t device, uint8_t func) {
 ahci_init(bus, device, func);
 uintptr_t abar = ahci_get_abar(bus, device, func);
 volatile uint32_t *hba = (volatile uint32_t *)PA_TO_VA(abar);
 kprintf("[AHCI] ABAR=%lx CAP=%08x PI=%08x VS=%08x\n", abar,
         hba[AHCI_CAP / 4], hba[AHCI_PI / 4], hba[AHCI_VS / 4]);
 hba[AHCI_GHC / 4] |= AHCI_GHC_AE;
 uint32_t pi = hba[AHCI_PI / 4];
 for (int i = 0; i < 32; i++) {
  ahci_port_t *pp = &g_ports[i];
  pp->regs = &hba[(AHCI_PORT_BASE + i * AHCI_PORT_SIZE) / 4];
  if (!(pi >> i & 1) || (PX(pp, AHCI_PX_SSTS) & 0xF) != 3)
   continue;
  uint32_t sig = PX(pp, AHCI_PX_SIG);
  kprintf("[AHCI] port%d TFD=%08x SIG=%08x SSTS=%08x\n", i,
          PX(pp, AHCI_PX_TFD), sig, PX(pp, AHCI_PX_SSTS));
  if (sig != AHCI_SIG_ATA) {
   kprintf("[AHCI] port%d: not ATA, skipped\n", i);
   continue;
  }
  ahci_port_init(pp);
  if (ahci_identify(pp))
   continue;
  g_ready |= 1u << i;
  if (!ahci_read(pp, 0, 1)) {
   kprintf("[AHCI] LBA0:");
   for (int j = 0; j < 8; j++)
    kprintf(" %c", ((volatile uint8_t *)pp->buf)[j]);
   kprintf("\n");
  }
 }
 return 0;
}

int ahci_read_sectors(int port, uint64_t lba, uint16_t count, void *dst) {
 if (port < 0 || port >= 32 || !(g_ready & (1u << port)))
  return -1;
 ahci_port_t *pt = &g_ports[port];
 uint8_t *out = dst;
 while (count) {
  uint16_t n = count > 8 ? 8 : count;
  if (ahci_read(pt, lba, n))
   return -1;
  for (uint32_t i = 0; i < n * 512u; i++)
   out[i] = ((volatile uint8_t *)pt->buf)[i];
  out += n * 512;
  lba += n;
  count -= n;
 }
 return 0;
}