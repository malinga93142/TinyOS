#include "../include/printk.h"
#include <stdint.h>
uint64_t getcr0() {
 uint64_t cr0;
 __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
 return cr0;
}
uint64_t getcr2() {
 uint64_t cr2;
 __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
 return cr2;
}
uint64_t getcr3() {
 uint64_t cr3;
 __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
 return cr3;
}
void setcr3(uint64_t value) {
 __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}
void setcr0(uint64_t value) {
 __asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}
uint64_t getcr4() {
 uint64_t cr4;
__asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
 return cr4;
}
void setcr4(uint64_t cr4) { __asm__ volatile("mov %0, %%cr4" ::"r"(cr4)); }
void print_cr3() {
 uint64_t cr3 = getcr3();
 kprintf("[KERNEL] CR3 = %x\n", cr3);
}
void print_cr4() {
 uint64_t cr4 = getcr4();
 kprintf("[KERNEL] CR4 = %x\n", cr4);
}
uint64_t get_pml4_phys(void) {
 uint64_t cr3 = getcr3();
 return cr3 & 0xFFFFFFFFFFFFF000; // Clear lower 12 bits
}
void cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b, uint32_t *c,
           uint32_t *d) {
  __asm__ volatile("cpuid"
                   : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                   : "a"(leaf), "c"(sub));
}