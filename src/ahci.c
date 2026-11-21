#include "../include/ahci.h"
#include "../include/pci.h"
#include "../include/printk.h"
#include "../include/paging.h"
#include "../include/hpet.h"
#include "../include/pmm.h"
#include <stdint.h>
#define PX(pt, off) ((pt)->regs[(off) / 4])
#define IDENTIFY_TIMEOUT_US 100000
static ahci_port_t g_ports[32];
static uint32_t g_ready;

static uintptr_t ahci_get_abar(uint8_t bus, uint8_t device, uint8_t func) {
 uint32_t bar5 = pci_config_read(bus, device, func, PCI_REG_BAR5);
 uint64_t pa = (uint64_t)(bar5 & 0xFFFFFFF0);
 paging_map_page((uint64_t)PA_TO_VA(pa), pa,
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
static void ahci_port_stop(ahci_port_t *pt) {
 PX(pt, AHCI_PX_CMD) &= ~AHCI_CMD_ST;
 while (PX(pt, AHCI_PX_CMD) & AHCI_CMD_CR)
  ;
 PX(pt, AHCI_PX_CMD) &= ~AHCI_CMD_FRE;
 while (PX(pt, AHCI_PX_CMD) & AHCI_CMD_FR)
  ;
}
static void ahci_port_init(volatile uint32_t *hba, int port, ahci_port_t *pt) {
 uint64_t cl_pa, fis_pa;
 pt->regs = &hba[(AHCI_PORT_BASE + port * AHCI_PORT_SIZE) / 4];
 ahci_port_stop(pt);
 pt->cl = alloc_zeroed(&cl_pa);
 alloc_zeroed(&fis_pa);
 pt->ct = alloc_zeroed(&pt->ct_pa);
 pt->buf = (volatile uint16_t *)alloc_zeroed(&pt->buf_pa);
 PX(pt, AHCI_PX_CLB) = (uint32_t)cl_pa;
 PX(pt, AHCI_PX_CLBU) = (uint32_t)(cl_pa >> 32);
 PX(pt, AHCI_PX_FB) = (uint32_t)fis_pa;
 PX(pt, AHCI_PX_FBU) = (uint32_t)(fis_pa >> 32);
 volatile uint32_t *hdr = (volatile uint32_t *)pt->cl;
 hdr[2] = (uint32_t)pt->ct_pa;
 hdr[3] = (uint32_t)(pt->ct_pa >> 32);
 PX(pt, AHCI_PX_SERR) = 0xFFFFFFFF;
 PX(pt, AHCI_PX_IS) = 0xFFFFFFFF;
 PX(pt, AHCI_PX_CMD) |= AHCI_CMD_FRE;
 PX(pt, AHCI_PX_CMD) |= AHCI_CMD_ST;
}
int ahci_identify(ahci_port_t *pt) {
 volatile uint32_t *hdr = (volatile uint32_t *)pt->cl;
 volatile uint32_t *prd = (volatile uint32_t *)(pt->ct + 0x80);
 hdr[0] = 5 | (1u << 16);
 hdr[1] = 0;
 for (int i = 0; i < 20; i++)
  pt->ct[i] = 0;
 pt->ct[0] = FIS_TYPE_H2D;
 pt->ct[1] = 0x80;
 pt->ct[2] = ATA_CMD_IDENTIFY;
 pt->ct[7] = 0x40;
 prd[0] = (uint32_t)pt->buf_pa;
 prd[1] = (uint32_t)(pt->buf_pa >> 32);
 prd[2] = 0;
 prd[3] = 511;
 PX(pt, AHCI_PX_IS) = 0xFFFFFFFF;
 asm volatile("" ::: "memory");
 PX(pt, AHCI_PX_CI) = 1;
 uint64_t start = hpet_get_counter();
 uint64_t limit = hpet_us_to_ticks(IDENTIFY_TIMEOUT_US);
 while (PX(pt, AHCI_PX_CI) & 1) {
  if (hpet_get_counter() - start > limit) {
   kprintf("[AHCI] IDENTIFY timeout: CI=%x IS=%x TFD=%x CMD=%x hdr=%x %x %x %x\n",
           PX(pt, AHCI_PX_CI), PX(pt, AHCI_PX_IS), PX(pt, AHCI_PX_TFD),
           PX(pt, AHCI_PX_CMD), hdr[0], hdr[1], hdr[2], hdr[3]);
   return -1;
  }
 }
 if (PX(pt, AHCI_PX_IS) & AHCI_IS_TFES) {
  kprintf("[AHCI] TFES (task file error)\n");
  return -1;
 }
 char model[41];
 for (int k = 0; k < 20; k++) {
  model[k * 2] = (pt->buf[27 + k] >> 8) & 0xFF;
  model[k * 2 + 1] = pt->buf[27 + k] & 0xFF;
 }
 model[40] = 0;
 for (int k = 39; k >= 0 && model[k] == ' '; k--)
  model[k] = 0;
 uint32_t lba28 = ((uint32_t)pt->buf[61] << 16) | pt->buf[60];
 kprintf("[AHCI] model = '%s'\n", model);
 kprintf("[AHCI] LBA28 = %u sectors\n", lba28);
 return 0;
}
void ahci_init(uint8_t bus, uint8_t device, uint8_t func) {
 uint16_t cmd = pci_read_word(bus, device, func, 0x04);
 pci_write_word(bus, device, func, 0x04, cmd | 0x06);
}
int ahci_probe(uint8_t bus, uint8_t device, uint8_t func) {
 ahci_init(bus, device, func);
 uintptr_t abar = ahci_get_abar(bus, device, func);
 volatile uint32_t *hba = (volatile uint32_t *)PA_TO_VA(abar);
 kprintf("[AHCI ABAR] %lx\n", abar);
 kprintf("[AHCI] CAP     = 0x%08x\n", hba[AHCI_CAP / 4]);
 kprintf("[AHCI] PI      = 0x%08x\n", hba[AHCI_PI / 4]);
 kprintf("[AHCI] VERSION = 0x%08x\n", hba[AHCI_VS / 4]);
 hba[AHCI_GHC / 4] |= AHCI_GHC_AE;
 uint32_t pi = hba[AHCI_PI / 4];
 for (int port = 0; port < 32; port++) {
  if (!(pi & (1u << port)))
   continue;
  ahci_port_t *pp = &g_ports[port];
  pp->regs = &hba[(AHCI_PORT_BASE + port * AHCI_PORT_SIZE) / 4];
  uint32_t ssts = PX(pp, AHCI_PX_SSTS);
  uint32_t sig = PX(pp, AHCI_PX_SIG);
  // pt.regs = &hba[(AHCI_PORT_BASE + port * AHCI_PORT_SIZE) / 4];
  // uint32_t ssts = PX(&pt, AHCI_PX_SSTS);
  // uint32_t sig = PX(&pt, AHCI_PX_SIG);
  if ((ssts & 0xF) != 3)
   continue;
  kprintf("[AHCI] port%d TFD=0x%08x SIG=0x%08x SSTS=0x%08x\n",
          port, PX(pp, AHCI_PX_TFD), sig, ssts);
  if (sig == AHCI_SIG_ATA) {
   ahci_port_init(hba, port, pp);
   if (ahci_identify(pp) == 0) {
    g_ready |= (1u << port);
   }
   if (ahci_read(pp, 0, 1) == 0) {
    volatile uint8_t *b = (volatile uint8_t *)pp->buf;
    kprintf("[AHCI] LBA0: ");
    for (int i = 0; i < 8; i++)
     kprintf(" %c", b[i]);
    kprintf("\n");
   }
  } else if (sig == AHCI_SIG_ATAPI) {
   kprintf("[AHCI] port%d: ATAPI, skipped\n", port);
  }
 }
 return 0;
}
int ahci_read(ahci_port_t *pt, uint64_t lba, uint16_t count) {
 if (count == 0 || count > 8)
  return -1;

 volatile uint32_t *hdr = (volatile uint32_t *)pt->cl;
 volatile uint32_t *prd = (volatile uint32_t *)(pt->ct + 0x80);

 for (int i = 0; i < 1024; i++)
  ((volatile uint32_t *)pt->buf)[i] = 0;

 hdr[0] = 5 | (1u << 16);
 hdr[1] = 0;

 for (int i = 0; i < 20; i++)
  pt->ct[i] = 0;
 pt->ct[0] = FIS_TYPE_H2D;
 pt->ct[1] = 0x80;
 pt->ct[2] = ATA_CMD_READ_DMA_EXT;
 pt->ct[4] = lba & 0xFF;
 pt->ct[5] = (lba >> 8) & 0xFF;
 pt->ct[6] = (lba >> 16) & 0xFF;
 pt->ct[7] = 0x40;
 pt->ct[8] = (lba >> 24) & 0xFF;
 pt->ct[9] = (lba >> 32) & 0xFF;
 pt->ct[10] = (lba >> 40) & 0xFF;
 pt->ct[12] = count & 0xFF;
 pt->ct[13] = count >> 8;

 prd[0] = (uint32_t)pt->buf_pa;
 prd[1] = (uint32_t)(pt->buf_pa >> 32);
 prd[2] = 0;
 prd[3] = count * 512 - 1;

 PX(pt, AHCI_PX_IS) = 0xFFFFFFFF;
 asm volatile("" ::: "memory");
 PX(pt, AHCI_PX_CI) = 1;

 uint64_t start = hpet_get_counter();
 uint64_t limit = hpet_us_to_ticks(IDENTIFY_TIMEOUT_US);
 while (PX(pt, AHCI_PX_CI) & 1) {
  if (hpet_get_counter() - start > limit) {
   kprintf("[AHCI] read timeout: CI=%x IS=%x TFD=%x\n",
           PX(pt, AHCI_PX_CI), PX(pt, AHCI_PX_IS), PX(pt, AHCI_PX_TFD));
   return -1;
  }
 }

 if (PX(pt, AHCI_PX_IS) & AHCI_IS_TFES) {
  kprintf("[AHCI] read: task file error\n");
  return -1;
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
  if (ahci_read(pt, lba, n) != 0)
   return -1;
  volatile uint8_t *src = (volatile uint8_t *)pt->buf;
  for (uint32_t i = 0; i < (uint32_t)n * 512; i++)
   out[i] = src[i];
  out += n * 512;
  lba += n;
  count -= n;
 }
 return 0;
}
