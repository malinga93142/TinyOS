#include "../include/smp.h"
#include "../include/cpu.h"
#include "../include/cpu_features.h"
#include "../include/hpet.h"
#include "../include/lapic.h"
#include "../include/paging.h"
#include <stdint.h>
#define AP_BASE 0x8000
#define STACK_SIZE 4096 // 4KB stack per AP (or 16KB if preferred)
#define MAX_APS 8       // Maximum expected cores

static uint8_t ap_stack_pool[MAX_APS][STACK_SIZE] __attribute__((aligned(16)));
volatile uint32_t ap_online;
extern void ap_main(void);
extern char ap_trampoline_start[];
extern char ap_trampoline_end[];
extern char ap_cr3_val[];
extern char ap_stack_val[];
extern char ap_main_addr[];
extern char ap_idt_ptr[];
void smp_bringup(uint32_t apic_id) {
  uint32_t size = ap_trampoline_end - ap_trampoline_start;
  volatile uint8_t *target_va = PA_TO_VA(AP_BASE);
  for (uint32_t i = 0; i < size; i++) {
    target_va[i] = ap_trampoline_start[i];
  }
  uintptr_t cr3_off = (uintptr_t)ap_cr3_val - (uintptr_t)ap_trampoline_start;
  uintptr_t stack_off =
      (uintptr_t)ap_stack_val - (uintptr_t)ap_trampoline_start;
  uintptr_t main_off = (uintptr_t)ap_main_addr - (uintptr_t)ap_trampoline_start;
  uintptr_t idt_off = (uintptr_t)&ap_idt_ptr - (uintptr_t)ap_trampoline_start;
  void *stack_top = (void *)ap_stack_pool[apic_id] + STACK_SIZE;

  *(uint32_t *)(target_va + cr3_off) = (uint32_t)VA_TO_PA(get_pml4_phys());
  *(uint64_t *)(target_va + stack_off) = (uint64_t)stack_top;
  *(uint64_t *)(target_va + main_off) = (uint64_t)ap_main;
  struct {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed)) idt_desc;
  __asm__ volatile("sidt %0" : "=m"(idt_desc));
  *(uint16_t *)(target_va + idt_off) = idt_desc.limit;
  *(uint64_t *)(target_va + idt_off + 2) = idt_desc.base;
  if (x2apic_supported()) {
    lapic_send_init_ipi_x2(apic_id);
    hpet_delay_us(10000);
    lapic_send_startup_ipi_x2(apic_id, 0x8);
  } else {
    lapic_send_init_ipi_mmio((uint8_t)apic_id);
    hpet_delay_us(10000);
    lapic_send_startup_ipi_mmio((uint8_t)apic_id, 0x8);
  }
  hpet_delay_us(20000);
}
