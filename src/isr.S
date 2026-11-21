.intel_syntax noprefix
.equ REG_SIZE,              8
.equ NUM_GP_REGS,           15
.equ PUSHALL_BYTES,         (NUM_GP_REGS * REG_SIZE)   // 120 bytes (15 * 8)
.equ CPU_AND_VEC_BYTES,     40                         // 24 (RIP, CS, RFLAGS) + 16 (Vector, ErrCode)
.equ CLEANUP_ERRVEC,        16
.extern handler
.macro PUSHALL
  push rax
  push rbx
  push rcx
  push rdx
  push rsi
  push rdi
  push rbp
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15
.endm

.macro POPALL
  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rbp
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop rbx
  pop rax
.endm


.macro ISR_ERR num
.globl isr\num
isr\num:
  push \num
  jmp isr_common
.endm
.macro ISR_NOERR num
.globl isr\num
isr\num:
  push 0
  push \num
  jmp isr_common
.endm

isr_common:
  PUSHALL
  mov rdi, rsp
  call handler
  POPALL
  add rsp, CLEANUP_ERRVEC
  iretq

/* --- CPU Exceptions (0 - 31) --- */
ISR_NOERR 0   // Division Error
ISR_NOERR 1   // Debug
ISR_NOERR 2   // Non-Maskable Interrupt
ISR_NOERR 3   // Breakpoint
ISR_NOERR 4   // Overflow
ISR_NOERR 5   // BOUND Range Exceeded
ISR_NOERR 6   // Invalid Opcode
ISR_NOERR 7   // Device Not Available (FPU)
ISR_ERR 8     // Double Fault (pushes error code)
ISR_NOERR 9   // Coprocessor Segment Overrun
ISR_ERR 10    // Invalid TSS
ISR_ERR 11    // Segment Not Present
ISR_ERR 12    // Stack-Segment Fault
ISR_ERR 13    // General Protection Fault
ISR_ERR 14    // Page Fault
ISR_NOERR 15  // Reserved
ISR_NOERR 16  // x87 Floating-Point Exception
ISR_ERR 17    // Alignment Check
ISR_NOERR 18  // Machine Check
ISR_NOERR 19  // SIMD Floating-Point Exception
ISR_NOERR 20  // Virtualization Exception
ISR_NOERR 21  // Control Protection Exception
ISR_NOERR 22  // Reserved
ISR_NOERR 23  // Reserved
ISR_NOERR 24  // Reserved
ISR_NOERR 25  // Reserved
ISR_NOERR 26  // Reserved
ISR_NOERR 27  // Reserved
ISR_NOERR 28  // Hypervisor Injection Exception
ISR_NOERR 29  // VMM Communication Exception
ISR_ERR 30    // Security Exception
ISR_NOERR 31  // Reserved
ISR_NOERR 33
// LAPIC vectors
ISR_NOERR 240   // LAPIC timer (LAPIC_VECTOR_TIMER)
ISR_NOERR 241   // LAPIC thermal
ISR_NOERR 242   // LAPIC perf counter
ISR_NOERR 243   // LAPIC CMCI
ISR_NOERR 253   // LAPIC IPI
ISR_NOERR 254   // LAPIC error
ISR_NOERR 255		 // LAPIC_SPURIOUS

.globl isr_stub_table
isr_stub_table:
  .quad isr0
  .quad isr1
  .quad isr2
  .quad isr3
  .quad isr4
  .quad isr5
  .quad isr6
  .quad isr7
  .quad isr8
  .quad isr9
  .quad isr10
  .quad isr11
  .quad isr12
  .quad isr13
  .quad isr14
  .quad isr15
  .quad isr16
  .quad isr17
  .quad isr18
  .quad isr19
  .quad isr20
  .quad isr21
  .quad isr22
  .quad isr23
  .quad isr24
  .quad isr25
  .quad isr26
  .quad isr27
  .quad isr28
  .quad isr29
  .quad isr30
  .quad isr31
  .quad isr33
.globl lapic_isr_table
lapic_isr_table:
	.quad isr240
	.quad isr241
	.quad isr242
	.quad isr243
	.quad isr253
	.quad isr254
	.quad isr255
