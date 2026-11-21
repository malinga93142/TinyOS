#include "../include/syscall.h"
#include "../include/msr.h"
#include "../include/printk.h"
#include "../include/task.h"

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

uint64_t syscall_ksp;
uint64_t syscall_usp;

uint64_t syscall_handler(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4) {
 (void)a1; (void)a2; (void)a3; (void)a4;
 switch (nr) {
 case SYS_EXIT:
  task_exit();
  return 0;
 case SYS_DEBUG:
  kprintf("[SYSCALL] a0=%lx\n", a0);
  return 0;
 case SYS_GETPID:
  return current->id;
 case SYS_WRITE:
  const char *m = (const char *)a0;
  kprintf("%s, %lx\n", m, a1);
  return 0;
 default:
  return (uint64_t)-1;
 }
 return 0;
}

void syscall_init(void) {
 wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);
 wrmsr(MSR_EFER, rdmsr(MSR_EFER) | (1ULL << 11));
 wrmsr(MSR_STAR, (0x18ULL << 48) | (0x08ULL << 32));
 wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
 wrmsr(MSR_FMASK, 0x700);
}
