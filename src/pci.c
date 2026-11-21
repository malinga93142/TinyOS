#include "../include/pci.h"
#include "../include/misc.h"
#include "../include/printk.h"
#include "../include/ahci.h"
#include <stdint.h>
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
 // E-bit | BUSN | DN | Function NUM | Reg Offset
 uint32_t addr = (uint32_t)((1U << 31)) | ((uint32_t)bus << 16)
                |((uint32_t)(device & 0x1F) << 11)
                |((uint32_t)(function & 0x07)<<8)
                |(offset & 0xfc);
 outl(0xCF8, addr);
 return inl(0xCFC);
}
void pci_write_word(uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t offset, uint16_t val) {
 uint32_t addr = (1U << 31) | ((uint32_t)bus << 16)
               | ((uint32_t)(device & 0x1F) << 11)
               | ((uint32_t)(function & 0x07) << 8)
               | (offset & 0xFC);
 outl(0xCF8, addr);
 uint32_t d = inl(0xCFC);
 int sh = (offset & 2) * 8;
 d = (d & ~(0xFFFFu << sh)) | ((uint32_t)val << sh);
 outl(0xCF8, addr);
 outl(0xCFC, d);
}

uint16_t pci_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
 uint32_t dword = pci_config_read(bus, device, function, offset & 0xFC);
 return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
 uint32_t dword = pci_config_read(bus, device, function, offset & 0xFC);
 return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}
// static void ahci_identify(volatile uint32_t *hba, int port);
void pci_check_device(uint8_t bus, uint8_t device) {
 uint16_t vendor_id = pci_read_word(bus, device, 0, 0x00);
 if (vendor_id == 0xffff)
  return;
 uint8_t header_type = pci_read_byte(bus, device, 0, 0x0e);
 uint8_t max_funcs = (header_type & 0x80) ? 8:1;
 for (uint8_t func = 0; func < max_funcs; func++) {
  uint16_t curr_vendor = pci_read_word(bus, device, func, 0x00);
  if (curr_vendor == 0xffff)
   continue;
  uint16_t device_id = pci_read_word(bus, device, func, 0x02);
  uint8_t class_code = pci_read_byte(bus, device, func, 0x0B);
  uint8_t subclass   = pci_read_byte(bus, device, func, 0x0A);
  uint8_t prog_if    = pci_read_byte(bus, device, func, 0x09);
  kprintf("[PCI] %x:%x.%x  %x:%x  class=%x:%x.%x\n",bus, device, func, curr_vendor,
          device_id, class_code, subclass, prog_if);
  if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
   kprintf("[PCI] AHCI Found\n");
   // uintptr_t abar = ahci_get_abar(bus, device, func);
   // kprintf("[AHCI ABAR] %lx\n", abar);
   // volatile uint32_t *hba = (volatile uint32_t *)PA_TO_VA(abar);
   // uint32_t cap = hba[0x00/4];
   // uint32_t pi  = hba[0x0C/4];
   // uint32_t ver = hba[0x10/4];
   // kprintf("[AHCI] CAP     = 0x%08x\n", cap);
   // kprintf("[AHCI] PI      = 0x%08x\n", pi);
   // kprintf("[AHCI] VERSION = 0x%08x\n", ver);
   // uint32_t bar4 = pci_config_read(bus, device, func, PCI_REG_BAR4);
   // kprintf("[AHCI] BAR4= %08x\n", bar4);
   // if (bar4 & 0x1) {
   //  uint16_t io_base = bar4 & 0xFFFC;
   //  kprintf("io_base: %08x\n", io_base);
   // }
   // uint16_t cmd = pci_read_word(bus, device, func, 0x04);
   // if (!(cmd & 1)) {
   //  kprintf("[IDE] enabled I/O decode\n");
   // } else {
   //  kprintf("[IDE] I/O decode already enabled\n");
   //  uint8_t st = inb(ATA_REG_COMMAND);
   //  kprintf("[IDE] status=0x%02x\n", st);
   // }
   // uint32_t port = 0x100;
   // uint32_t px_tfd  = hba[(port + 0x20) / 4];   /* task file data */
   // uint32_t px_sig  = hba[(port + 0x24) / 4];   /* signature */
   // uint32_t px_ssts = hba[(port + 0x28) / 4];   /* SATA status */
   // 
   // kprintf("[AHCI] port0 TFD  = 0x%08x\n", px_tfd);
   // kprintf("[AHCI] port0 SIG  = 0x%08x\n", px_sig);
   // kprintf("[AHCI] port0 SSTS = 0x%08x\n", px_ssts);
   // 
   // uint32_t det = px_ssts & 0xF;
   // kprintf("[AHCI] DET = %u (3 = link up)\n", det);
   // 
   // if (px_sig == 0x00000101)
   //  kprintf("[AHCI] ATA drive present\n");
   // else if (px_sig == 0xEB140101)
   //  kprintf("[AHCI] ATAPI drive present\n");
   // else if (px_sig == 0xFFFFFFFF)
   //  kprintf("[AHCI] no drive on port 0\n");
   // ahci_identify(hba,  0);
   ahci_probe(bus, device, func);
   // ahci_identify()
  }
  if (curr_vendor == 0x1234 && device_id == 0x5678) {
   kprintf("[PCI] mydev found at %x:%x.%x");
  }
 }
}

void pci_enumerate_bus(void) {
 for (uint16_t bus = 0; bus < 256; bus++) {
  for (uint8_t device = 0; device < 32; device++) {
   pci_check_device((uint8_t)bus, device);
  }
 }
 kprintf("[PCI] Enumeration complete.\n");
}
