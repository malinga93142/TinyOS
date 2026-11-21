#include "../include/percpu.h"
#include "../include/kstring.h"
#include "../include/msr.h"
#define IA32_GS_BASE 0xC0000101
extern char __percpu_start[], __percpu_end[];
extern struct gdt_struct my_gdt;
uint8_t percpu_area[MAX_CPUS][PERCPU_SIZE] __attribute__((aligned(64)));

DEFINE_PERCPU(uint32_t, cpu_idx);
DEFINE_PERCPU(uint32_t, apic_id);
DEFINE_PERCPU(uint32_t, test_val);
DEFINE_PERCPU(uint8_t, is_bsp);
void percpu_init(uint32_t idx, uint32_t apic, int bsp) {
 size_t sz = __percpu_end - __percpu_start;
 uint8_t *blk = percpu_area[idx];
 kmemcpy(blk, __percpu_start, sz);
 wrmsr(IA32_GS_BASE, (uint64_t)(uintptr_t)blk);
 percpu_write(cpu_idx, idx);
 percpu_write(apic_id, apic);
 percpu_write(is_bsp, bsp);
}
