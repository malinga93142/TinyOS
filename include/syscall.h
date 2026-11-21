#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

#define SYS_EXIT 0
#define SYS_DEBUG 1
#define SYS_GETPID 2
#define SYS_WRITE 3
extern uint64_t syscall_ksp;
extern uint64_t syscall_usp;
void syscall_entry(void);
void syscall_init(void);
#endif
