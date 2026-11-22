#include "../include/idt.h"
#include "../include/kstring.h"
#include "../include/printk.h"
#include <stdint.h>
struct idt_entry IDT[256] = {0};
static struct idtr idtptr;
void add_idt(struct idt_entry *__idt, uint8_t ist_idx, uint64_t handler,
                    uint8_t type) {
  __idt->offset_low = handler & 0xffff;
  __idt->selector = 0x08; // this is hard coded
  __idt->ist = ist_idx;
  __idt->type_attr = type;
  __idt->offset_mid = (handler >> 16) & 0xffff;
  __idt->offset_high = (handler >> 32);
  __idt->zero = 0;
}

// declare the table of stubs defined in your assembly file. look in file isr.S
extern void *isr_stub_table[35];
extern void *lapic_isr_table[7];

void init_idt(void) {
  kmemset((void *)&IDT, 0, sizeof(IDT));
  for (int i = 0; i < 33; i++) {
    if (i == 8) {
      add_idt(&IDT[i], 1, (uint64_t)(uintptr_t)isr_stub_table[i],
              IDT_INTERRUPT_GATE | IDT_PRESENT);
      continue;
    } else if (i == 2) {
      add_idt(&IDT[i], 2, (uint64_t)(uintptr_t)isr_stub_table[i],
              IDT_INTERRUPT_GATE | IDT_PRESENT);
      continue;
    }
    add_idt(&IDT[i], 0, (uint64_t)(uintptr_t)isr_stub_table[i],
            IDT_INTERRUPT_GATE | IDT_PRESENT);
  }
  add_idt(&IDT[240], 0, (uint64_t)(uintptr_t)lapic_isr_table[0],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  add_idt(&IDT[241], 0, (uint64_t)(uintptr_t)lapic_isr_table[1],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  add_idt(&IDT[242], 0, (uint64_t)(uintptr_t)lapic_isr_table[2],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  add_idt(&IDT[243], 0, (uintptr_t)lapic_isr_table[3],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  add_idt(&IDT[253], 0, (uintptr_t)lapic_isr_table[4],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  add_idt(&IDT[254], 0, (uintptr_t)lapic_isr_table[5],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  add_idt(&IDT[255], 0, (uintptr_t)lapic_isr_table[6],
          IDT_INTERRUPT_GATE | IDT_PRESENT);
  idtptr.base = (uint64_t)&IDT;
  idtptr.limit = sizeof(IDT) - 1;
  asm volatile("lidt %0" ::"m"(idtptr) : "memory");

  kprintf("[KERNEL] IDT INIT (Registered 32 CPU Exceptions)\n");
}
