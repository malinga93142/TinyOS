#ifndef PERCPU_H
#define PERCPU_H
#include <stdint.h>

#define MAX_CPUS 8
#define PERCPU_SIZE (2 * 4096)

extern char __percpu_start[], __percpu_end[];
extern uint8_t percpu_area[MAX_CPUS][PERCPU_SIZE]
    __attribute__((aligned(4096)));

#define __percpu __attribute__((section(".percpu")))

#define DECLARE_PERCPU(type, name) extern __percpu type name
#define DEFINE_PERCPU(type, name) __percpu type name
#define PERCPU_OFF(var) ((uintptr_t)&(var) - (uintptr_t)__percpu_start)

#define PERCPU_CHECK_SIZE(var)                                                 \
  _Static_assert(sizeof(var) == 1 || sizeof(var) == 2 || sizeof(var) == 4 ||   \
                     sizeof(var) == 8,                                         \
                 "percpu: unsupported size")

// this core's copy, via GS
#define percpu_read(var)                                                       \
  ({                                                                           \
    PERCPU_CHECK_SIZE(var);                                                    \
    __typeof__(var) __v;                                                       \
    __asm__ volatile("mov %%gs:(%1), %0" : "=r"(__v) : "r"(PERCPU_OFF(var)));  \
    __v;                                                                       \
  })

#define percpu_write(var, val)                                                 \
  do {                                                                         \
    PERCPU_CHECK_SIZE(var);                                                    \
    __typeof__(var) __w = (val);                                               \
    __asm__ volatile("mov %0, %%gs:(%1)"                                       \
                     :                                                         \
                     : "r"(__w), "r"(PERCPU_OFF(var))                          \
                     : "memory");                                              \
  } while (0)

// another core's copy
#define percpu_remote(idx, var)                                                \
  ((__typeof__(var) *)(percpu_area[(idx)] + PERCPU_OFF(var)))

#define percpu_remote_read(idx, var)                                           \
  __atomic_load_n(percpu_remote(idx, var), __ATOMIC_ACQUIRE)
#define percpu_remote_write(idx, var, val)                                     \
  __atomic_store_n(percpu_remote(idx, var), (val), __ATOMIC_RELEASE)
#define percpu_remote_add(idx, var, val)                                       \
  __atomic_fetch_add(percpu_remote(idx, var), (val), __ATOMIC_SEQ_CST)

// built-in per-cpu variables (defined in percpu.c)
DECLARE_PERCPU(uint32_t, cpu_idx);
DECLARE_PERCPU(uint32_t, apic_id);
DECLARE_PERCPU(uint32_t, test_val);
DECLARE_PERCPU(uint8_t, is_bsp);

#define cpu_id() percpu_read(cpu_idx)
#define cpu_apic_id() percpu_read(apic_id)
#define cpu_is_bsp() percpu_read(is_bsp)
#define this_cpu() cpu_id()
// runs on the core being initialised, sets that core's GS base
void percpu_init(uint32_t idx, uint32_t apic, int bsp);

#endif // PERCPU_H
