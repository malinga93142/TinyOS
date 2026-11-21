
#include "../include/gdt.h"
#include "../include/kstring.h"
#include "../include/percpu.h"
#include "../include/printk.h"
#include "../include/syscall.h"
#include <stddef.h>
#include <stdint.h>

#define KSTACK_SIZE (4096 * 5)
#define DF_STACK_SIZE (4096 * 5)
#define NMI_STACK_SIZE (4096 * 5)

struct gdt_struct {
 struct gdt_entry64 gdts[10];
 struct gdt_system_entry64 tss_desc;
 struct gdtr gdt_ptr;
} __attribute__((aligned(16)));

#define TSS_SEL ((uint16_t)offsetof(struct gdt_struct, tss_desc))
_Static_assert(offsetof(struct gdt_struct, tss_desc) == 0x50, "TSS selector");

static DEFINE_PERCPU(struct tss64_struct, cpu_tss) __attribute__((aligned(16)));
static DEFINE_PERCPU(struct gdt_struct, cpu_gdt) __attribute__((aligned(16)));

static uint8_t kstack[MAX_CPUS][KSTACK_SIZE] __attribute__((aligned(16)));
static uint8_t dfstack[MAX_CPUS][DF_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t nmistack[MAX_CPUS][NMI_STACK_SIZE] __attribute__((aligned(16)));

static inline void lgdt(struct gdtr gdt_ptr) {
  asm volatile("lgdt %0\n\t"
               "mov $0x10, %%ax\n\t"
               "mov %%ax, %%ds\n\t"
               "mov %%ax, %%es\n\t"
               "mov %%ax, %%ss\n\t"
               "pushq $0x08\n\t"
               "lea 1f(%%rip), %%rax\n\t"
               "pushq %%rax\n\t"
               "lretq\n\t"
               "1:\n" ::"m"(gdt_ptr)
               : "rax", "memory");
}
static void ltr(uint16_t idx) { __asm__ volatile("ltr %%ax" ::"a"(idx)); }

void gdt_init_cpu(uint32_t id) {
  if (id >= MAX_CPUS)
    return;

  struct gdt_struct *g = percpu_remote(id, cpu_gdt);
  struct tss64_struct *t = percpu_remote(id, cpu_tss);

  kmemset(t, 0, sizeof(*t));
  t->rsp0 = (uint64_t)(uintptr_t)(kstack[id] + KSTACK_SIZE);
  t->ist1 = (uint64_t)(uintptr_t)(dfstack[id] + DF_STACK_SIZE);   // vector 8
  t->ist2 = (uint64_t)(uintptr_t)(nmistack[id] + NMI_STACK_SIZE); // vector 2
  t->iomap_base = sizeof(*t);

  g->gdts[0] = (struct gdt_entry64){0};
  g->gdts[1] = (struct gdt_entry64){
      .limit_low = 0xffff, .access = 0x9A, .granularity = 0xAF};
  g->gdts[2] = (struct gdt_entry64){
      .limit_low = 0xffff, .access = 0x92, .granularity = 0xCF};
  g->gdts[3] = (struct gdt_entry64){
      .limit_low = 0xffff, .access = 0xFA, .granularity = 0xCF};
  g->gdts[4] = (struct gdt_entry64){
      .limit_low = 0xffff, .access = 0xF2, .granularity = 0xCF};
  g->gdts[5] = (struct gdt_entry64){
      .limit_low = 0xffff, .access = 0xFA, .granularity = 0xAF};
  uint64_t base = (uint64_t)(uintptr_t)t; // the copy, never &cpu_tss
  uint32_t limit = sizeof(*t) - 1;
  g->tss_desc =
      (struct gdt_system_entry64){.limit_low = limit & 0xFFFF,
                                  .base_low = base & 0xFFFF,
                                  .base_mid = (base >> 16) & 0xFF,
                                  .access = 0x89,
                                  .granularity = (limit >> 16) & 0x0F,
                                  .base_high = (base >> 24) & 0xFF,
                                  .base_upper = (uint32_t)(base >> 32),
                                  .reserved = 0};

  g->gdt_ptr.base = (uint64_t)(uintptr_t)&g->gdts[0];
  g->gdt_ptr.limit = sizeof(g->gdts) + sizeof(g->tss_desc) - 1;

  lgdt(g->gdt_ptr);
  ltr(TSS_SEL);
}
void init_gdt(void) {
  gdt_init_cpu(0);
  kprintf("[KERNEL] GDT/TSS INIT (cpu 0)\n");
}
void tss_set_rsp0(uint64_t rsp0) {
 percpu_remote(cpu_id(), cpu_tss)->rsp0 = rsp0;
 syscall_ksp =rsp0;
}
