#include "../include/acpi.h"
#include "../include/mb.h"

__attribute__((section("text.parse"))) int __preparse(uint32_t mbinfo,
                                                      uint32_t mbaddr) {
  (void)mbinfo;
  void *rsdp = mb_find_acpi_rsdp((struct mb_info *)(uintptr_t)mbaddr);
  if (rsdp) {
    int is_xsdt;

    struct acpi_sdt_header *root = acpi_get_root_table(rsdp, &is_xsdt);
    struct acpi_sdt_header *madt = acpi_find_table(root, is_xsdt, "APIC");
    if (madt) {
      struct cpu_topology topo;
      acpi_parse_madt((struct acpi_madt *)madt, &topo);
    }
  }
  return 0;
}
