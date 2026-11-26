#include "../include/cpu.h"
#include "../include/cpu_features.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/lapic.h"
#include "../include/hpet.h"
#include "../include/percpu.h"
#include "../include/printk.h"
#include "../include/uart.h"
#include "../include/acpi.h"
#include "../include/task.h"
#include <stdatomic.h>
#include <stdint.h>
extern _Atomic int kernel_ready;
extern _Atomic int ap_halt;
// returns this CPU's APIC id
static uint32_t ap_lapic_init(void) {
  uint32_t id;
  if (x2apic_supported()) {
    kprintf("[KERNEL] x2APIC is supported\n");
    lapic_init_x2();
    id = lapic_get_id_x2();
  } else {
    kprintf("[KERNEL] LAPIC is supported\n");
    lapic_init_mmio();
    id = lapic_get_id_mmio();
  }
  kprintf("[KERNEL] LAPIC INIT\n");
  return id;
}
static void ap_percpu_init(uint32_t apic_id) {
  percpu_init(apic_id, apic_id, 0);
  gdt_init_cpu(apic_id);
  kprintf("[CPU %d] PERCPU INIT\n", apic_id);
}
static void ap_kernel_init(void) {
  uart_init();
  // init_gdt();
  init_idt();
}
static void ap_cpu_architecture_init(void) {
  print_cr3();
  if (pcid_support() == -1)
    kprintf("[KERNEL] No PCID Support\n");
  if (invpcid_support() == -1)
    kprintf("[KERNEL] No INVPCID Support\n");
  init_cpu_extended_states();
  enable_fsbase_in_cr4();
}

static void lookatcurrentask() {
 struct task *now = current;
 kprintf("id:%d, cr3=%lx\n", now->id, now->cr3);
}
static void* somefunc(void *arg) {
 (void)arg;
 kprintf("Bhalo\n");
 return NULL;
}
__attribute__((noreturn)) void ap_main(void) {
  // wait for the BSP to finish boot
  while (!atomic_load_explicit(&kernel_ready, memory_order_acquire))
    asm volatile("pause");
  ap_kernel_init();
  kprintf("[KERNEL] AP MAIN\n");
  ap_cpu_architecture_init();
  uint32_t apic_id = ap_lapic_init();
  ap_percpu_init(apic_id);
  percpu_write(test_val, 0x20);
  lapic_lvt_init();
  lapic_timer_calibrate();
  lapic_timer_start_periodic(2000);
  sti();
  struct task *t = task_create(somefunc, NULL);
  // lookatcurrentask();
  hpet_delay_us(1000);
  kprintf("%d\n", lapic_get_ticks());
  kprintf("[AP %d] HALT\n", cpu_id());
  hpet_delay_us(9000);
  atomic_store_explicit(&ap_halt, 1, memory_order_release);
  while (1) { asm volatile("pause"); hlt(); kprintf("break\n"); break;}
  while (1) { asm volatile("pause"); hlt();}
}
