#ifndef CPU_H
#define CPU_H
#include <stdint.h>
#define sti() asm volatile("sti")
#define hlt() asm volatile("hlt")
uint64_t getcr0();
uint64_t getcr2();
uint64_t getcr3();
void setcr3(uint64_t);
void setcr4(uint64_t);
void setcr0(uint64_t);
uint64_t getcr4();
uint64_t get_pml4_phys(void);
void print_cr3();
void print_cr4();
void cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b, uint32_t *c,
           uint32_t *d);
#endif
