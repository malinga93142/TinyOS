#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// PCI Configuration Ports
#define PCI_CONFIG_ADDRESS 0x0CF8
#define PCI_CONFIG_DATA 0x0CFC

// Common PCI Register Offsets
#define PCI_REG_VENDOR_ID 0x00
#define PCI_REG_DEVICE_ID 0x02
#define PCI_REG_COMMAND 0x04
#define PCI_REG_STATUS 0x06
#define PCI_REG_REVISION_ID 0x08
#define PCI_REG_PROG_IF 0x09
#define PCI_REG_SUBCLASS 0x0A
#define PCI_REG_CLASS_CODE 0x0B
#define PCI_REG_HEADER_TYPE 0x0E
#define PCI_REG_BAR0 0x10
#define PCI_REG_BAR1 0x14
#define PCI_REG_BAR2 0x18
#define PCI_REG_BAR3 0x1C
#define PCI_REG_BAR4 0x20
#define PCI_REG_BAR5 0x24

// Class Codes
#define PCI_CLASS_MASS_STORAGE 0x01

// Mass Storage Subclasses
#define PCI_SUBCLASS_IDE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_SUBCLASS_NVME 0x08

// SATA / AHCI Program Interfaces
#define PCI_PROGIF_AHCI 0x01

// Function Declarations
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function,
                         uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset, uint32_t value);

uint16_t pci_read_word(uint8_t bus, uint8_t device, uint8_t function,
                       uint8_t offset);
uint8_t pci_read_byte(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset);

void pci_check_device(uint8_t bus, uint8_t device);
void pci_enumerate_bus(void);

#endif  // PCI_H
