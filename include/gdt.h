#ifndef GDT_H
#define GDT_H
#include <stddef.h>
#include <stdint.h>
#define SEL_KCODE 0x08
#define SEL_KDATA 0x10
#define SEL_UDATA (0x20 | 3)
#define SEL_UCODE (0x28 | 3)

struct gdt_entry64 {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed));
_Static_assert(sizeof(struct gdt_entry64) == 0x8, "must be 8 bytes");

struct gdt_system_entry64 {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
  uint32_t base_upper;
  uint32_t reserved;
} __attribute__((packed));
_Static_assert(sizeof(struct gdt_system_entry64) == 0x10, "must be 16 bytes");

struct gdtr {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

struct tss64_struct {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;

  uint64_t reserved3;
  uint16_t reserved4;

  uint16_t iomap_base;
} __attribute__((packed));

_Static_assert(sizeof(struct tss64_struct) == 104,
               "64-bit tss must be 104 bytes");
_Static_assert(offsetof(struct tss64_struct, rsp0) == 0x4,
               "rsp0 must be at offset 0x4");
_Static_assert(offsetof(struct tss64_struct, ist1) == 0x24,
               "ist1 must be at offset 0x24");
_Static_assert(offsetof(struct tss64_struct, iomap_base) == 0x66,
               "iomap_base must be at offset 0x66");
void init_gdt(void);
void init_gdt_percpu(uint32_t cpu_id);
void gdt_init_cpu(uint32_t id);
void tss_set_rsp0(uint64_t);
#endif
