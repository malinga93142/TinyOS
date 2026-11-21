#include "../include/cpu.h"
#include "../include/ef.h"
#include "../include/kbd.h"
#include "../include/lapic.h"
#include "../include/paging.h"
#include "../include/percpu.h"
#include "../include/printk.h"
#include "../include/task.h"
#include "../include/gfx.h"
#include "../include/vmm.h"

DEFINE_PERCPU(uint32_t, nmi_count);
DEFINE_PERCPU(uint8_t, nmi_pending);

#define USER_END 0x0000800000000000ULL

static void handle_page_fault(struct exception_frame *ef) {
  uint64_t cr2 = getcr2();
  uint64_t err = ef->error_code;
  int present = err & 0x1;
  int rw = (err >> 1) & 0x1;
  int us = (err >> 2) & 0x1;

  if (cr2 < USER_END && current->mm) {
    if (mm_handle_fault(current->mm, cr2, err) == 0)
      return;
    kprintf("[#PF] bad access addr=%lx rip=%lx err=%lx task=%u\n", cr2,
            ef->rip, err, current->id);
    if (!us)
      panic("[#PF] kernel fault on bad user address");
    task_exit();
  }

  kprintf("[EXCEPTION] Page Fault (#PF) at virtual address %lx\n", cr2);
  kprintf(" Cause: %s, %s, %s\n",
          present ? "Protection Violation" : "Page Not Present",
          rw ? "Write Operation" : "Read Operation",
          us ? "User-mode" : "Kernel-mode");
  kprintf(" CPU= %d RIP= %lx RSP= %lx ERROR= %lx\n", cpu_id(), ef->rip, ef->rsp,
          err);
  page_fault_map(cr2);
  kprintf("[PF HANDLER] Address %lx Mapped.\n", cr2);
}

static void handle_double_fault(struct exception_frame *ef) {
  kprintf("[KERNEL PANIC] START");
  kprintf("[KERNEL PANIC] Double Fault (#DF) encountered!\n");
  kprintf("  Error Code: %x (Always 0)\n", ef->error_code);
  kprintf("  RIP: %lx | CS: %lx | RFLAGS: %lx\n", ef->rip, ef->cs, ef->rflags);
  panic("[KERNEL PANIC] END");
}

static void handle_gpf(struct exception_frame *ef) {
  kprintf("[KERNEL PANIC] General Protection Fault (#GP)\n");
  kprintf("  Error Code (Selector/Vector): %x\n", ef->error_code);
  kprintf("  RIP: %lx\n", ef->rip);
  panic("[KERNEL PANIC] END");
}
static void handle_invalid_opcode(struct exception_frame *ef) {
  kprintf("[KERNEL PANIC] START\n");
  kprintf("[KERNEL PANIC] Invalid Opcode (#UD) at RIP= %lx, RSP= %lx\n", ef->rip,
          ef->rsp);
  kprintf(" Check if tried using SSE/AVX/ZMM before enabling "
          "CR4.OSXSAVE or XCR0!\n");
  panic("[KERNEL PANIC] END");
}
static void nmi_handler(struct exception_frame *ef) {
  percpu_write(nmi_count, percpu_read(nmi_count) + 1);
  kprintf("[CPU %d] NMI: RSP= %lx RIP= %lx CPU0= %lx CPU1= %d\n", cpu_id(),
          ef->rsp, ef->rip, *percpu_remote(0, nmi_count),
          *percpu_remote(1, nmi_count));
  percpu_write(nmi_pending, 1);
}
void ap_wakeup_handler(void) {
    __asm__ volatile ("wrmsr" : : "c"(X2APIC_MSR_EOI), "a"(0U), "d"(0U));
}
void handler(struct exception_frame *ef) {
  switch (ef->vector) {
  case 0:
    kprintf("[#DE] cpu=%d rip=%lx rsp=%lx cs=%lx\n", cpu_id(), ef->rip, ef->rsp,
            ef->cs);
    for (;;)
      asm volatile("cli; hlt");
    break;
  case 2:
    nmi_handler(ef);
    break;
  case 6:
    handle_invalid_opcode(ef);
    break;
  case 7:
    kprintf("[EXCEPTION] Device Not Available (FPU/Vector missing)\n");
    break;
  case 8:
    handle_double_fault(ef);
    break;
  case 13:
    handle_gpf(ef);
    break;
  case 14:
    handle_page_fault(ef);
    break;
  case 16:
    panic("[EXCEPTION] x87 Floating-Point Exception\n");
    break;
  case 19:
    panic("[EXCEPTION] SIMD Floating-Point Exception\n");
    break;
  case KBD_VECTOR:
   // puts_fb("fired\n");
   kbd_irq();
   break;
  case 240:
    // kprintf("LAPIC TIMER\n");
    lapic_timer_handler();
    break;
  case 241:
    kprintf("LAPIC THERMAL\n");
    break;
  case 242:
    kprintf("LAPIC PERF COUNTER\n");
    break;
  case 243:
    kprintf("LAPIC CMCI\n");
    break;
  case 253:
   ap_wakeup_handler();
   break;
  case 254:
    kprintf("LAPIC ERROR\n");
    break;
  case 255:
    kprintf("LAPIC SPURIOUS\n");
    break;
  default: {
    kprintf("[EXCEPTION] Unhandled Exception Vector: %d (Error Code: %x)\n",
            (int)ef->vector, ef->error_code);
    kprintf("  RIP: %lx | RSP: %lx\n", ef->rip, ef->rsp);
    panic("[EXCEPTION] PANIC");
    break;
  }
  }
}
