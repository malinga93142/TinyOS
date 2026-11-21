#include "pci.h"
#include "../include/misc.h"
#include "../include/printk.h"
uint32_t
pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
  uint32_t address
      = (uint32_t)((1U << 31) |                         // E-bit
                   ((uint32_t)bus << 16) |              // BUSN
                   ((uint32_t)(device & 0x1F) << 11) |  // DN (5 bits)
                   ((uint32_t)(function & 0x07) << 8)
                   |                // Function number (3 bits)
                   (offset & 0xFC)  // Register offset (aligned)
      );

  outl(0xCF8, address);
  return inl(0xCFC);
}
uint16_t
pci_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
  uint32_t dword = pci_config_read(bus, device, function, offset & 0xFC);
  return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t
pci_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
  uint32_t dword = pci_config_read(bus, device, function, offset & 0xFC);
  return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}
void
pci_check_device(uint8_t bus, uint8_t device)
{
  uint16_t vendor_id = pci_read_word(bus, device, 0, 0x00);
  if (vendor_id == 0xFFFF)
    {
      return;  // Device doesn't exist
    }

  // Check if it's a multi-function device
  uint8_t header_type = pci_read_byte(bus, device, 0, 0x0E);
  uint8_t max_functions = (header_type & 0x80) ? 8 : 1;

  for (uint8_t func = 0; func < max_functions; func++)
    {
      uint16_t curr_vendor = pci_read_word(bus, device, func, 0x00);
      if (curr_vendor == 0xFFFF)
        continue;

      // Read Class Code, Subclass, and Prog IF to identify device type
      uint8_t class_code = pci_read_byte(bus, device, func, 0x0B);
      uint8_t subclass = pci_read_byte(bus, device, func, 0x0A);
      uint8_t prog_if = pci_read_byte(bus, device, func, 0x09);
      // TODO: Print or store device info
      // e.g., if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) ->
      // Found AHCI!
      if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01)
        {
          kprintf("[PCI] AHCI Found\n");
        }
    }
}

void
pci_enumerate_bus(void)
{
  for (uint16_t bus = 0; bus < 256; bus++)
    {
      for (uint8_t device = 0; device < 32; device++)
        {
          pci_check_device((uint8_t)bus, device);
        }
    }
}
