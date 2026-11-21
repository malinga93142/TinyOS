#ifndef AHCI_H
#define AHCI_H
#include <stdint.h>
// HBA registers (byte offsets)
#define AHCI_CAP         0x00
#define AHCI_GHC         0x04
#define AHCI_PI          0x0C
#define AHCI_VS          0x10
#define AHCI_GHC_AE      (1u << 31)
// per-port registers
#define AHCI_PORT_BASE   0x100
#define AHCI_PORT_SIZE   0x80
#define AHCI_PX_CLB      0x00
#define AHCI_PX_CLBU     0x04
#define AHCI_PX_FB       0x08
#define AHCI_PX_FBU      0x0C
#define AHCI_PX_IS       0x10
#define AHCI_PX_CMD      0x18
#define AHCI_PX_TFD      0x20
#define AHCI_PX_SIG      0x24
#define AHCI_PX_SSTS     0x28
#define AHCI_PX_SERR     0x30
#define AHCI_PX_CI       0x38

#define AHCI_CMD_ST      (1u << 0)
#define AHCI_CMD_FRE     (1u << 4)
#define AHCI_CMD_FR      (1u << 14)
#define AHCI_CMD_CR      (1u << 15)
#define AHCI_IS_TFES     (1u << 30)

#define AHCI_SIG_ATA     0x00000101
#define AHCI_SIG_ATAPI   0xEB140101

#define FIS_TYPE_H2D     0x27
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_CMD_READ_DMA_EXT 0x25


typedef struct {
 volatile uint32_t *regs;   /* this port's register block */
 volatile uint8_t  *cl;
 volatile uint8_t  *ct;
 volatile uint16_t *buf;
 uint64_t ct_pa;
 uint64_t buf_pa;
} ahci_port_t;


int ahci_probe(uint8_t bus, uint8_t device, uint8_t func);
int ahci_identify(ahci_port_t *pt);
void ahci_init(uint8_t bus, uint8_t device, uint8_t func);
int ahci_read(ahci_port_t *pt, uint64_t lba, uint16_t count);
int ahci_read_sectors(int port, uint64_t lba, uint16_t count, void *dst);

#endif
