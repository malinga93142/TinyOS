#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#define IDT_PRESENT 0x80
#define IDT_INTERRUPT_GATE 0x0E
#define IDT_TRAP_GATE 0x8E

struct idt_entry {
  uint16_t offset_low;  // offset bits 0-15
  uint16_t selector;    // cs selector
  uint8_t ist;          // interrupt stack table + reserved
  uint8_t type_attr;    // type and attributes
  uint16_t offset_mid;  // offset bits 16-31
  uint32_t offset_high; // offset bits 32-63
  uint32_t zero;        // reserved
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 0x10, "must be 16 bytes");

struct idtr {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));
extern struct idt_entry IDT[256];
_Static_assert(sizeof(struct idtr) == 0xa, "must be 10 bytes");
void init_idt(void);
void add_idt(struct idt_entry *__idt, uint8_t ist_idx, uint64_t handler,
                    uint8_t type);
/*
// we do not need these extern'd functions declarations.
// CPU Exception Service Routines (0 - 31)
extern void isr0(void);  // Division Error
extern void isr1(void);  // Debug
extern void isr2(void);  // Non-Maskable Interrupt
extern void isr3(void);  // Breakpoint
extern void isr4(void);  // Overflow
extern void isr5(void);  // BOUND Range Exceeded
extern void isr6(void);  // Invalid Opcode
extern void isr7(void);  // Device Not Available (FPU)
extern void isr8(void);  // Double Fault
extern void isr9(void);  // Coprocessor Segment Overrun
extern void isr10(void); // Invalid TSS
extern void isr11(void); // Segment Not Present
extern void isr12(void); // Stack-Segment Fault
extern void isr13(void); // General Protection Fault
extern void isr14(void); // Page Fault
extern void isr15(void); // Reserved
extern void isr16(void); // x87 Floating-Point Exception
extern void isr17(void); // Alignment Check
extern void isr18(void); // Machine Check
extern void isr19(void); // SIMD Floating-Point Exception
extern void isr20(void); // Virtualization Exception
extern void isr21(void); // Control Protection Exception
extern void isr22(void); // Reserved
extern void isr23(void); // Reserved
extern void isr24(void); // Reserved
extern void isr25(void); // Reserved
extern void isr26(void); // Reserved
extern void isr27(void); // Reserved
extern void isr28(void); // Hypervisor Injection Exception
extern void isr29(void); // VMM Communication Exception
extern void isr30(void); // Security Exception
extern void isr31(void); // Reserved
*/

#endif // IDT_H
