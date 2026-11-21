#ifndef ACPI_H
#define ACPI_H
#include <stdint.h>
struct acpi_rsdp {
  char signature[8]; /* "RSD PTR " */
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision_id; /* 0 = ACPI 1.0, 2 = ACPI 2.0+ */
  uint32_t rsdt_address;
} __attribute__((packed));
struct acpi_rsdp2 {
  struct acpi_rsdp v1; /* ACPI 1.0 fields, unchanged */
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t extended_chksum;
  uint8_t reserved[3];
} __attribute__((packed));
struct acpi_sdt_header {
  char signature[4]; /* e.g. "APIC" for MADT */
  uint32_t length;
  uint8_t revision;
  uint8_t chksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed));
int acpi_chksum_ok(const void *table, uint32_t length);
struct acpi_sdt_header *acpi_get_root_table(void *rsdp_ptr, int *out_is_xsdt);
struct acpi_sdt_header *acpi_find_table(struct acpi_sdt_header *root,
                                        int is_xsdt, const char sig[4]);
struct acpi_madt {
  struct acpi_sdt_header header;
  uint32_t lapic_address;
  uint32_t flags;
} __attribute__((packed));
struct madt_entry_header {
  uint8_t type;
  uint8_t length;
} __attribute__((packed));
#define MADT_TYPE_LOCAL_APIC 0
struct madt_local_apic {
  struct madt_entry_header header;
  uint8_t acpi_processor_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__((packed));

// HPET Struct
struct acpi_hpet {
  struct acpi_sdt_header header;
  uint8_t hardware_rev_id;
  uint8_t comparator_count : 5;
  uint8_t counter_size_64 : 1;
  uint8_t reserved : 1;
  uint8_t legacy_replacement : 1;
  uint16_t pci_vendor_id;
  struct {
    uint8_t address_space_id; // 0 = system memory, 1 = system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address; // Physical base address of HPET
  } __attribute__((packed)) address;
  uint8_t hpet_number;
  uint16_t minimum_tick;
  uint8_t page_protection;
} __attribute__((packed));
#define MADT_MAX_CPUS 32
struct cpu_topology {
  uint32_t lapic_address;
  uint32_t cpu_count;
  uint8_t apic_ids[MADT_MAX_CPUS];
};
extern struct cpu_topology topo;
void acpi_parse_madt(struct acpi_madt *madt, struct cpu_topology *out);
#endif /* ACPI_H */
