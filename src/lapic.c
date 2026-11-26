#include "../include/lapic.h"
#include "../include/config.h"
#include "../include/hpet.h"
#include "../include/mm.h"
#include "../include/printk.h"
#include "../include/msr.h"
#include "../include/paging.h"
#include "../include/percpu.h"
#include "../include/task.h"
#include <stdint.h>

static int lapic_x2_mode = 0;
static uint32_t lapic_ticks_per_ms = 0;
static volatile uint8_t *lapic_base = 0;

static inline void lapic_write_mmio(uint32_t reg, uint32_t v) {
 *(volatile uint32_t *)(lapic_base + reg) = v;
}

static inline uint32_t lapic_read_mmio(uint32_t reg) {
 return *(volatile uint32_t *)(lapic_base + reg);
}

void lapic_eoi_mmio(void) { lapic_write_mmio(LAPIC_REG_EOI, 0); }

void lapic_nmi_enable_mmio(void) {
 lapic_write_mmio(LAPIC_REG_LVT_LINT1, LAPIC_LVT_NMI);
}

void lapic_nmi_disable_mmio(void) {
 lapic_write_mmio(LAPIC_REG_LVT_LINT1, LAPIC_LVT_MASKED);
}

void lapic_init_mmio(void) {
 uint64_t apic_msr = rdmsr(IA32_APIC_BASE_MSR);
 apic_msr |= APIC_BASE_EN;
 wrmsr(IA32_APIC_BASE_MSR, apic_msr);
 uintptr_t phys_base = (uintptr_t)(apic_msr & 0xFFFFF000);
 uintptr_t virt_base = (uintptr_t)PA_TO_VA(phys_base);
 paging_map_page(virt_base, phys_base,
                 PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
 lapic_base = (volatile uint8_t *)virt_base;
 lapic_x2_mode = 0;
 lapic_write_mmio(LAPIC_REG_SPURIOUS, lapic_read_mmio(LAPIC_REG_SPURIOUS) |
                                          LAPIC_SPUR_ENABLE | 0xFF);
#ifdef CONFIG_LAPIC_NMI
 lapic_nmi_enable_mmio();
#endif
}

uint32_t lapic_get_id_mmio(void) { return lapic_read_mmio(LAPIC_REG_ID) >> 24; }

void lapic_send_nmi_mmio(uint8_t apic_id) {
 lapic_write_mmio(LAPIC_REG_ICR_HIGH, (uint32_t)apic_id << 24);
 lapic_write_mmio(LAPIC_REG_ICR_LOW, LAPIC_ICR_NMI | LAPIC_ICR_ASSERT);
 while (lapic_read_mmio(LAPIC_REG_ICR_LOW) & LAPIC_ICR_BUSY)
  ;
}

void lapic_send_init_ipi_mmio(uint8_t apic_id) {
 lapic_write_mmio(LAPIC_REG_ICR_HIGH, (uint32_t)apic_id << 24);
 uint32_t icr_low = LAPIC_ICR_INIT | LAPIC_ICR_LEVEL | LAPIC_ICR_ASSERT;
 lapic_write_mmio(LAPIC_REG_ICR_LOW, icr_low);
 while (lapic_read_mmio(LAPIC_REG_ICR_LOW) & LAPIC_ICR_BUSY)
  ;
}

void lapic_send_startup_ipi_mmio(uint8_t apic_id, uint8_t v) {
 lapic_write_mmio(LAPIC_REG_ICR_HIGH, (uint32_t)apic_id << 24);
 uint32_t icr_low = LAPIC_ICR_STARTUP | (v & LAPIC_ICR_VECTOR);
 lapic_write_mmio(LAPIC_REG_ICR_LOW, icr_low);
 while (lapic_read_mmio(LAPIC_REG_ICR_LOW) & LAPIC_ICR_BUSY)
  ;
}

static inline void lapic_write_x2(uint32_t reg, uint64_t v) {
 wrmsr(X2APIC_MSR(reg), v);
}

static inline uint64_t lapic_read_x2(uint32_t reg) {
 return rdmsr(X2APIC_MSR(reg));
}

void lapic_eoi_x2(void) { lapic_write_x2(LAPIC_REG_EOI, 0); }

void lapic_nmi_enable_x2(void) {
 lapic_write_x2(LAPIC_REG_LVT_LINT1, LAPIC_LVT_NMI);
}

void lapic_nmi_disable_x2(void) {
 lapic_write_x2(LAPIC_REG_LVT_LINT1, LAPIC_LVT_MASKED);
}

void lapic_init_x2(void) {
 uint64_t base = rdmsr(IA32_APIC_BASE_MSR);
 if (!(base & APIC_BASE_EN)) {
  base |= APIC_BASE_EN;
  wrmsr(IA32_APIC_BASE_MSR, base);
 }
 base |= APIC_BASE_EXTD;
 wrmsr(IA32_APIC_BASE_MSR, base);
 lapic_x2_mode = 1;
 lapic_write_x2(LAPIC_REG_SPURIOUS, LAPIC_SPUR_ENABLE | 0xFF);
#ifdef CONFIG_LAPIC_NMI
 lapic_nmi_enable_x2();
#endif
}

uint32_t lapic_get_id_x2(void) { return (uint32_t)lapic_read_x2(LAPIC_REG_ID); }

uint32_t lapic_timer_read_current(void) {
 return (uint32_t)lapic_read_x2(X2APIC_MSR(LAPIC_REG_TIMER_CUR));
}

void lapic_send_nmi_x2(uint32_t apic_id) {
 lapic_write_x2(LAPIC_REG_ICR_LOW,
                ((uint64_t)apic_id << 32) | LAPIC_ICR_NMI | LAPIC_ICR_ASSERT);
}

void lapic_send_ipi_x2(uint32_t apic_id, uint8_t vector) {
 lapic_write_x2(LAPIC_REG_ICR_LOW, ((uint64_t)apic_id << 32) |
                                       LAPIC_ICR_FIXED | LAPIC_ICR_ASSERT |
                                       vector);
}

void lapic_send_init_ipi_x2(uint32_t apic_id) {
 lapic_write_x2(LAPIC_REG_ICR_LOW, ((uint64_t)apic_id << 32) | LAPIC_ICR_INIT |
                                       LAPIC_ICR_LEVEL | LAPIC_ICR_ASSERT);
}

void lapic_send_startup_ipi_x2(uint32_t apic_id, uint8_t vector) {
 lapic_write_x2(LAPIC_REG_ICR_LOW,
                ((uint64_t)apic_id << 32) | LAPIC_ICR_STARTUP | vector);
}

static inline void lapic_write(uint32_t reg, uint32_t v) {
 if (lapic_x2_mode) {
  lapic_write_x2(reg, v);
 }
 else
  lapic_write_mmio(reg, v);
}

static inline uint32_t lapic_read(uint32_t reg) {
 return lapic_x2_mode ? (uint32_t)lapic_read_x2(reg) : lapic_read_mmio(reg);
}

void lapic_eoi(void) {
 if (lapic_x2_mode)
  lapic_eoi_x2();
 else
  lapic_eoi_mmio();
}

void lapic_enable(void) {
 uint32_t v = lapic_read(LAPIC_REG_SPURIOUS);
 v &= ~LAPIC_SPUR_VECTOR;
 v |= LAPIC_SPUR_ENABLE | LAPIC_VECTOR_SPURIOUS;
 lapic_write(LAPIC_REG_SPURIOUS, v);
}

void lapic_lvt_init(void) {
 uint32_t max_lvt = LAPIC_VER_MAXLVT(lapic_read(LAPIC_REG_VERSION));
 lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED | LAPIC_VECTOR_TIMER);
 lapic_write(LAPIC_REG_LVT_LINT0, LAPIC_LVT_MASKED);
 if (max_lvt >= 4)
  lapic_write(LAPIC_REG_LVT_PERF, LAPIC_LVT_MASKED | LAPIC_VECTOR_PERF);
 if (max_lvt >= 5)
  lapic_write(LAPIC_REG_LVT_THERMAL, LAPIC_LVT_MASKED | LAPIC_VECTOR_THERMAL);
 if (max_lvt >= 6)
  lapic_write(LAPIC_REG_LVT_CMCI, LAPIC_LVT_MASKED | LAPIC_VECTOR_CMCI);
 lapic_write(LAPIC_REG_LVT_ERROR, LAPIC_VECTOR_ERROR);
 lapic_write(LAPIC_REG_ESR, 0);
 (void)lapic_read(LAPIC_REG_ESR);
}

#define LAPIC_PIT_FREQ 1193182
#define LAPIC_PIT_CAL_MS 10
#define LAPIC_PIT_CAL_COUNT (LAPIC_PIT_FREQ * LAPIC_PIT_CAL_MS / 1000)
#define LAPIC_CAL_MS 10

static inline void lapic_outb(uint16_t port, uint8_t v) {
 __asm__ volatile("outb %0, %1" ::"a"(v), "Nd"(port));
}

static inline uint8_t lapic_inb(uint16_t port) {
 uint8_t v;
 __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
 return v;
}

DEFINE_PERCPU(uint64_t, lapic_ticks);

void lapic_timer_calibrate(void) {
 if (!hpet_is_ready())
  return;
 lapic_write(LAPIC_REG_TIMER_DIV, LAPIC_TIMER_DIV_16);
 lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED | LAPIC_VECTOR_TIMER);
 lapic_write(LAPIC_REG_TIMER_INIT, 0xFFFFFFFF);
 hpet_delay_us(LAPIC_CAL_MS * 1000);
 uint32_t elapsed = 0xFFFFFFFF - lapic_read(LAPIC_REG_TIMER_CUR);
 lapic_write(LAPIC_REG_TIMER_INIT, 0);
 lapic_ticks_per_ms = elapsed / LAPIC_CAL_MS;
}

uint32_t lapic_timer_ticks_per_ms(void) { return lapic_ticks_per_ms; }

void lapic_timer_start_periodic(uint32_t hz) {
 if (!lapic_ticks_per_ms || !hz)
  return;
 uint32_t count = (uint32_t)(((uint64_t)lapic_ticks_per_ms * 1000) / hz);
 lapic_write(LAPIC_REG_TIMER_DIV, LAPIC_TIMER_DIV_16);
 lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | LAPIC_VECTOR_TIMER);
 lapic_write(LAPIC_REG_TIMER_INIT, count);
}

void lapic_timer_stop(void) {
 lapic_write(LAPIC_REG_TIMER_INIT, 0);
 lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED | LAPIC_VECTOR_TIMER);
}

int lapic_timer_start(uint32_t hz) {
 if (!lapic_ticks_per_ms || !hz)
  return -1;
 uint64_t count = ((uint64_t)lapic_ticks_per_ms * 1000) / hz;
 if (count == 0)
  count = 1;
 if (count > 0xFFFFFFFFULL)
  count = 0xFFFFFFFFULL;
 lapic_write(LAPIC_REG_TIMER_INIT, 0);
 lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED | LAPIC_VECTOR_TIMER);
 percpu_write(lapic_ticks, 0);
 lapic_write(LAPIC_REG_TIMER_DIV, LAPIC_TIMER_DIV_16);
 lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | LAPIC_VECTOR_TIMER);
 lapic_write(LAPIC_REG_TIMER_INIT, (uint32_t)count);
 return 0;
}

#define TASK_SLICE_TICKS 100
#define LAPIC_TICK_LIMIT 10000000

void lapic_timer_handler(void) {
 uint64_t t = percpu_read(lapic_ticks) + 1;
 percpu_write(lapic_ticks, t);
 if (t >= LAPIC_TICK_LIMIT)
  lapic_timer_stop();
 lapic_eoi();
 // if (cpu_is_bsp() && t % TASK_SLICE_TICKS == 0)
  // task_schedule();
}

uint64_t lapic_get_ticks(void) {
	uint64_t v = percpu_read(lapic_ticks);
	// kprintf("%d\n", v);
	asm volatile("":::"memory");
	return v;
}

/* IPI */


static inline void
x2apic_write64(uint32_t msr, uint64_t val) {
 uint32_t lo = (uint32_t)val;
 uint32_t hi = (uint32_t)(val >> 32);
 asm volatile("wrmsr"::"c"(msr), "a"(lo), "d"(hi));
}
void x2apic_send_ipi(uint32_t target_x2apic_id, uint8_t vector) {
 uint64_t icr = 0;
 icr = ((uint64_t)target_x2apic_id << 32)
  | (0U << 15)
  | (1U << 14)
  | (0U <<  8)
  | (uint32_t)vector;
 x2apic_write64(X2APIC_MSR_ICR, icr);
}

void lapic_enable_extint(void) {
 lapic_write(LAPIC_REG_LVT_LINT0, LAPIC_DM_EXTINT);
}