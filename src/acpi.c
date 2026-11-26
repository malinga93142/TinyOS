#include "../include/acpi.h"
#include "../include/misc.h"
#include <stdint.h>

int acpi_chksum_ok(const void *table, uint32_t length) {
 const uint8_t *bytes = (const uint8_t *)table;
 uint8_t sum = 0;
 for (uint32_t i = 0; i < length; i++)
  sum = (uint8_t)(sum + bytes[i]);
 return sum == 0;
}

struct acpi_sdt_header *acpi_get_root_table(void *rsdp_ptr, int *out_is_xsdt) {
 struct acpi_rsdp *rsdp = (struct acpi_rsdp *)rsdp_ptr;

 if (!acpi_chksum_ok(rsdp, sizeof(struct acpi_rsdp)))
  return 0; // not a real RSDP

 if (rsdp->revision_id >= 2) {
  struct acpi_rsdp2 *rsdp2 = (struct acpi_rsdp2 *)rsdp_ptr;
  if (!acpi_chksum_ok(rsdp2, rsdp2->length))
   return 0; // extended region is corrupt.
  *out_is_xsdt = 1;
  return (struct acpi_sdt_header *)(uintptr_t)rsdp2->xsdt_address;
 }
 *out_is_xsdt = 0;
 return (struct acpi_sdt_header *)(uintptr_t)rsdp->rsdt_address;
}

struct acpi_sdt_header *acpi_find_table(struct acpi_sdt_header *root,
                                        int is_xsdt, const char sig[4]) {
 if (!root || !acpi_chksum_ok(root, root->length))
  return 0;

 uint8_t *entries = (uint8_t *)root + sizeof(struct acpi_sdt_header);
 uint32_t entry_size = is_xsdt ? 8 : 4;
 uint32_t count = (root->length - sizeof(struct acpi_sdt_header)) / entry_size;

 for (uint32_t i = 0; i < count; i++) {
  uintptr_t addr;
  if (is_xsdt) {
   addr = (uintptr_t)(*(uint64_t *)(entries + i * entry_size));
  } else {
   addr = (uintptr_t)(*(uint32_t *)(entries + i * entry_size));
  }
  struct acpi_sdt_header *table = (struct acpi_sdt_header *)addr;
  if (table->signature[0] == sig[0] && table->signature[1] == sig[1] &&
   table->signature[2] == sig[2] && table->signature[3] == sig[3]) {
   if (acpi_chksum_ok(table, table->length))
    return table;
  }
 }
 return 0;
}

void acpi_parse_madt(struct acpi_madt *madt, struct cpu_topology *out) {
 out->lapic_address = madt->lapic_address;
 out->cpu_count = 0;

 uint8_t *ptr = (uint8_t *)madt + sizeof(struct acpi_madt);
 uint8_t *end = (uint8_t *)madt + madt->header.length;

 while (ptr < end) {
  struct madt_entry_header *entry = (struct madt_entry_header *)ptr;

  if (entry->type == MADT_TYPE_LOCAL_APIC) {
   struct madt_local_apic *lapic = (struct madt_local_apic *)entry;
   if ((lapic->flags & 0x1) && out->cpu_count < MADT_MAX_CPUS) {
    out->apic_ids[out->cpu_count] = lapic->apic_id;
    out->cpu_count++;
   } else if (entry->type == 1) {
    outb(0x3f8, '1');
    while (1) asm volatile("hlt");
   } else if (entry->type == 2) {
    while(1)
     asm volatile("hlt");
   }
  }
  ptr += entry->length;
 }
}
