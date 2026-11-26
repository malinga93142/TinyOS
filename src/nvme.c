#include "../include/pci.h"
#include "../include/nvme.h"
#include "../include/paging.h"
#include "../include/printk.h"
#include <stdint.h>

int nvme_probe(uint8_t bus, uint8_t dev, uint8_t func) {
 pci_write_word(bus, dev, func, 0x04, pci_read_word(bus, dev,  func,  0x4) | 0x06);
 uint32_t lo = pci_config_read(bus, dev, func, 0x10);
 uint32_t hi = pci_config_read(bus, dev, func, 0x14);
 if ((lo & 0x6) != 0x4) {
  kprintf("[NVMe] BAR0 is not 64-bit memory\n");
  return -1;
 }
 uint64_t pa = ((uint64_t)hi << 32) | (lo & 0xFFFFFFF0);
 for (int i = 0; i < 4; i++) {
  paging_map_page((uint64_t)PA_TO_VA(pa + i * (PAGE_SIZE)),  pa + i * PAGE_SIZE, PAGE_PRESENT | PAGE_PWT | PAGE_PCD | PAGE_WRITE);
 }
 volatile uint32_t *r = (volatile uint32_t *)PA_TO_VA(pa);
 uint32_t cap_lo = r[NVME_REG_CAP / 4], cap_hi = r[NVME_REG_CAP / 4 + 1];
 uint32_t vs = r[NVME_REG_VS / 4];

 kprintf("[NVMe] BAR0=%lx VS=%u.%u\n", pa, vs >> 0x10, (vs >> 0x8) & 0xff);
 kprintf("[NVMe] max queue entries=%u timeout=%u ms  doorbell stride=%u\n",
  (cap_lo & 0xffff)+1, ((cap_lo >> 0x18) & 0xff)*500, cap_hi & 0xf);
 kprintf("[NVMe] CC.EN=%u CSTS.RDY=%u\n", r[NVME_REG_CC / 4] & 1, r[NVME_REG_CSTS / 4] & 1);
 return 0;
}