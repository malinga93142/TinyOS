#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

/* IA32_APIC_BASE */
#define IA32_APIC_BASE_MSR     0x0000001B
#define APIC_BASE_BSP          (1ULL << 8)
#define APIC_BASE_EXTD         (1ULL << 10)
#define APIC_BASE_EN           (1ULL << 11)
#define APIC_BASE_MASK         0x000FFFFFFFFFF000ULL
#define IA32_X2APIC_ENABLE APIC_BASE_EXTD

/* CPUID */
#define CPUID_VENDOR                0x00000000
#define CPUID_FEATURES_1            0x00000001
#define CPUID_FEATURES_1_EDX_APIC   (1u << 9)
#define CPUID_FEATURES_1_ECX_X2APIC (1u << 21)
#define CPUID_X2APIC_LEAF           0x0000000B

/* Register offsets (xAPIC MMIO; x2APIC derives MSRs from these) */
#define LAPIC_REG_ID             0x020
#define LAPIC_REG_VERSION        0x030
#define LAPIC_REG_TPR            0x080
#define LAPIC_REG_APR            0x090
#define LAPIC_REG_PPR            0x0A0
#define LAPIC_REG_EOI            0x0B0
#define LAPIC_REG_RRD            0x0C0
#define LAPIC_REG_LDR            0x0D0
#define LAPIC_REG_DFR            0x0E0 /* xAPIC only */
#define LAPIC_REG_SPURIOUS       0x0F0
#define LAPIC_REG_ISR_BASE       0x100
#define LAPIC_REG_TMR_BASE       0x180
#define LAPIC_REG_IRR_BASE       0x200
#define LAPIC_REG_ESR            0x280
#define LAPIC_REG_LVT_CMCI       0x2F0
#define LAPIC_REG_ICR_LOW        0x300  /* whole 64-bit ICR in x2APIC */
#define LAPIC_REG_ICR_HIGH       0x310  /* xAPIC only */
#define LAPIC_REG_LVT_TIMER      0x320
#define LAPIC_REG_LVT_THERMAL    0x330
#define LAPIC_REG_LVT_PERF       0x340
#define LAPIC_REG_LVT_LINT0      0x350
#define LAPIC_REG_LVT_LINT1      0x360
#define LAPIC_REG_LVT_ERROR      0x370
#define LAPIC_REG_TIMER_INIT     0x380
#define LAPIC_REG_TIMER_CUR      0x390
#define LAPIC_REG_TIMER_DIV      0x3E0
#define LAPIC_REG_SELF_IPI       0x3F0 /* x2APIC only */

/* Version register */
#define LAPIC_VER_MAXLVT(v) (((v) >> 16) & 0xFF)

/* Spurious vector register  */
#define LAPIC_SPUR_VECTOR  0x000000FF
#define LAPIC_SPUR_ENABLE  (1u << 8)
#define LAPIC_SPUR_FOCUS   (1u << 9)

/* LVT */
#define LAPIC_LVT_VECTOR   0x000000FF
#define LAPIC_LVT_NMI      0x00000400
#define LAPIC_LVT_PENDING  0x00004000
#define LAPIC_LVT_MASKED   0x00010000

#define LAPIC_DM_FIXED      0x00000000
#define LAPIC_DM_SMI        0x00000200
#define LAPIC_DM_INIT       0x00000500
#define LAPIC_DM_EXTINT     0x00000700

/* Timer */
#define LAPIC_TIMER_ONESHOT     0x00000000
#define LAPIC_TIMER_PERIODIC    0x00020000
#define LAPIC_TIMER_TSCDEADLINE 0x00040000

#define LAPIC_TIMER_DIV_1   0x0B
#define LAPIC_TIMER_DIV_2   0x00
#define LAPIC_TIMER_DIV_4   0x01
#define LAPIC_TIMER_DIV_8   0x02
#define LAPIC_TIMER_DIV_16  0x03
#define LAPIC_TIMER_DIV_32  0x08
#define LAPIC_TIMER_DIV_64  0x09
#define LAPIC_TIMER_DIV_128 0x0A

/* ICR */
#define LAPIC_ICR_VECTOR  0x000000FF
#define LAPIC_ICR_FIXED   0x00000000
#define LAPIC_ICR_NMI     0x00000400
#define LAPIC_ICR_INIT    0x00000500
#define LAPIC_ICR_STARTUP 0x00000600

#define LAPIC_ICR_PHYSICAL 0x00000000
#define LAPIC_ICR_LOGICAL  0x00000800

#define LAPIC_ICR_DS_IDLE 0x00000000 /* xAPIC only */
#define LAPIC_ICR_DS_SEND 0x00001000 /* xAPIC only */
#define LAPIC_ICR_BUSY LAPIC_ICR_DS_SEND

#define LAPIC_ICR_ASSERT 0x00004000
#define LAPIC_ICR_LEVEL  0x00008000
#define LAPIC_ICR_TRIG_LEVEL LAPIC_ICR_LEVEL

#define LAPIC_ICR_DST_NONE   0x00000000
#define LAPIC_ICR_DST_SELF   0x00040000
#define LAPIC_ICR_DST_ALL    0x00080000
#define LAPIC_ICR_DST_OTHERS 0x000C0000

/* Error status register  */
#define LAPIC_ESR_SEND_CHECKSUM  (1u << 0)
#define LAPIC_ESR_RECV_CHECKSUM  (1u << 1)
#define LAPIC_ESR_SEND_ACCEPT    (1u << 2)
#define LAPIC_ESR_RECV_ACCEPT    (1u << 3)
#define LAPIC_ESR_REDIRECT       (1u << 4)
#define LAPIC_ESR_ILLEGAL_VECTOR (1u << 6)
#define LAPIC_ESR_ILLEGAL_ADDR   (1u << 7)

/* Interrupt vectors (must match the IDT) */
#define LAPIC_VECTOR_TIMER    0xF0
#define LAPIC_VECTOR_THERMAL  0xF1
#define LAPIC_VECTOR_PERF     0xF2
#define LAPIC_VECTOR_CMCI     0xF3
#define LAPIC_VECTOR_ERROR    0xFE
#define LAPIC_VECTOR_SPURIOUS 0xFF

/* x2APIC MSR map: 0x800 + (mmio_offset >> 4) */
#define X2APIC_MSR(off) (0x800 + ((off) >> 4))

#define X2APIC_MSR_ID           X2APIC_MSR(LAPIC_REG_ID)
#define X2APIC_MSR_VERSION      X2APIC_MSR(LAPIC_REG_VERSION)
#define X2APIC_MSR_TPR          X2APIC_MSR(LAPIC_REG_TPR)
#define X2APIC_MSR_PPR          X2APIC_MSR(LAPIC_REG_PPR)
#define X2APIC_MSR_EOI          X2APIC_MSR(LAPIC_REG_EOI)
#define X2APIC_MSR_LDR          X2APIC_MSR(LAPIC_REG_LDR)
#define X2APIC_MSR_SPURIOUS     X2APIC_MSR(LAPIC_REG_SPURIOUS)
#define X2APIC_MSR_ESR          X2APIC_MSR(LAPIC_REG_ESR)
#define X2APIC_MSR_LVT_CMCI     X2APIC_MSR(LAPIC_REG_LVT_CMCI)
#define X2APIC_MSR_ICR          X2APIC_MSR(LAPIC_REG_ICR_LOW)
#define X2APIC_MSR_LVT_TIMER    X2APIC_MSR(LAPIC_REG_LVT_TIMER)
#define X2APIC_MSR_LVT_THERMAL  X2APIC_MSR(LAPIC_REG_LVT_THERMAL)
#define X2APIC_MSR_LVT_PERF     X2APIC_MSR(LAPIC_REG_LVT_PERF)
#define X2APIC_MSR_LVT_LINT0    X2APIC_MSR(LAPIC_REG_LVT_LINT0)
#define X2APIC_MSR_LVT_LINT1    X2APIC_MSR(LAPIC_REG_LVT_LINT1)
#define X2APIC_MSR_LVT_ERROR    X2APIC_MSR(LAPIC_REG_LVT_ERROR)
#define X2APIC_MSR_TIMER_INIT   X2APIC_MSR(LAPIC_REG_TIMER_INIT)
#define X2APIC_MSR_TIMER_CUR    X2APIC_MSR(LAPIC_REG_TIMER_CUR)
#define X2APIC_MSR_TIMER_DIV    X2APIC_MSR(LAPIC_REG_TIMER_DIV)
#define X2APIC_MSR_SELF_IPI     X2APIC_MSR(LAPIC_REG_SELF_IPI)

/* Software IPI vectors (adjust to match IDT) */
#define LAPIC_VECTOR_IPI_TLB      0xF8U
#define LAPIC_VECTOR_IPI_RESCHED  0xF9U
#define LAPIC_VECTOR_IPI_WAKE     0xFAU
#define LAPIC_VECTOR_IPI_HALT     0xFBU

/* ICR bit helpers for x2APIC (64-bit: bits 63:32 = 32-bit x2APIC ID, bits 31:0 = control/vector) */
#define LAPIC_ICR_TRIG_EDGE       0x00000000U
#define LAPIC_ICR_DEASSERT        0x00000000U
#define LAPIC_ICR_DEST_SHIFT      32U
#define LAPIC_ICR_DEST(id)        ((uint64_t)(uint32_t)(id) << LAPIC_ICR_DEST_SHIFT)
#define LAPIC_ICR_VEC(vec)        ((uint64_t)((uint32_t)(vec) & LAPIC_ICR_VECTOR))

/* Base 64-bit ICR composers */
#define LAPIC_ICR_X2_FIXED(apic_id, vec) \
 (LAPIC_ICR_DEST(apic_id) | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_FIXED | LAPIC_ICR_VEC(vec))

#define LAPIC_ICR_X2_SELF(vec) \
 (LAPIC_ICR_DST_SELF | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_FIXED | LAPIC_ICR_VEC(vec))

#define LAPIC_ICR_X2_ALL(vec) \
 (LAPIC_ICR_DST_ALL | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_FIXED | LAPIC_ICR_VEC(vec))

#define LAPIC_ICR_X2_OTHERS(vec) \
 (LAPIC_ICR_DST_OTHERS | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_FIXED | LAPIC_ICR_VEC(vec))

#define LAPIC_ICR_X2_NMI(apic_id) \
 (LAPIC_ICR_DEST(apic_id) | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_NMI)

#define LAPIC_ICR_X2_NMI_OTHERS() \
 (LAPIC_ICR_DST_OTHERS | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_NMI)

#define LAPIC_ICR_X2_INIT(apic_id) \
 (LAPIC_ICR_DEST(apic_id) | LAPIC_ICR_TRIG_LEVEL | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_INIT)

#define LAPIC_ICR_X2_SIPI(apic_id, page_4k) \
 (LAPIC_ICR_DEST(apic_id) | LAPIC_ICR_TRIG_EDGE | LAPIC_ICR_ASSERT | \
  LAPIC_ICR_PHYSICAL | LAPIC_ICR_STARTUP | LAPIC_ICR_VEC(page_4k))

/* Specific IPI ICR values */
#define LAPIC_ICR_X2_TLB_SHOOTDOWN_ALL_OTHERS() LAPIC_ICR_X2_OTHERS(LAPIC_VECTOR_IPI_TLB)
#define LAPIC_ICR_X2_TLB_SHOOTDOWN_CPU(apic_id) LAPIC_ICR_X2_FIXED((apic_id), LAPIC_VECTOR_IPI_TLB)
#define LAPIC_ICR_X2_RESCHED_CPU(apic_id)       LAPIC_ICR_X2_FIXED((apic_id), LAPIC_VECTOR_IPI_RESCHED)
#define LAPIC_ICR_X2_RESCHED_OTHERS()           LAPIC_ICR_X2_OTHERS(LAPIC_VECTOR_IPI_RESCHED)
#define LAPIC_ICR_X2_WAKE_CPU(apic_id)          LAPIC_ICR_X2_FIXED((apic_id), LAPIC_VECTOR_IPI_WAKE)
#define LAPIC_ICR_X2_HALT_OTHERS()              LAPIC_ICR_X2_OTHERS(LAPIC_VECTOR_IPI_HALT)
/* Self-IPI MSR (0x83F) value */
#define LAPIC_SELF_IPI_VEC(vec) ((uint32_t)(vec) & LAPIC_ICR_VECTOR)


/* xAPIC (MMIO) */
void lapic_init_mmio(void);
uint32_t lapic_get_id_mmio(void);
void lapic_eoi_mmio(void);
void lapic_nmi_enable_mmio(void);
void lapic_nmi_disable_mmio(void);
void lapic_send_nmi_mmio(uint8_t apic_id);
void lapic_send_init_ipi_mmio(uint8_t apic_id);
void lapic_send_startup_ipi_mmio(uint8_t apic_id, uint8_t vector);

/* x2APIC (MSR) */
void lapic_init_x2(void);
uint32_t lapic_get_id_x2(void);
void lapic_eoi_x2(void);
void lapic_nmi_enable_x2(void);
void lapic_nmi_disable_x2(void);
void lapic_send_nmi_x2(uint32_t apic_id);
void lapic_send_ipi_x2(uint32_t apic_id, uint8_t vector);
void lapic_send_init_ipi_x2(uint32_t apic_id);
void lapic_send_startup_ipi_x2(uint32_t apic_id, uint8_t vector);
uint32_t lapic_timer_read_current(void);

/* Common (either mode) */
void lapic_eoi(void);
void lapic_enable(void);
void lapic_lvt_init(void);
void lapic_timer_calibrate(void);
uint32_t lapic_timer_ticks_per_ms(void);
void lapic_timer_start_periodic(uint32_t hz);
int lapic_timer_start(uint32_t hz);
void lapic_timer_stop(void);
void lapic_timer_handler(void);
uint64_t lapic_get_ticks(void);
void lapic_enable_extint(void);

/* IPI */
void x2apic_send_ipi(uint32_t target_x2apic_id, uint8_t vector);
#endif
