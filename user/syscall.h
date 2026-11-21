#include <stddef.h>
static inline long syscall1(long n, long a) {
 long r;
 __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a)
                  : "rcx", "r11", "memory");
 return r;
}
static inline long syscall3(long n, void *__ptr, size_t len) {
 long r;
 __asm__ volatile("syscall": "=a"(r)
  : "a"(n), "D"(__ptr), "S"(len)
  : "rcx", "r11", "memory");
 return r;
}
static inline long syscall4(long n, long a0, long a1, long a2) {
 long r;
 __asm__ volatile("syscall"
                  : "=a"(r)
                  : "a"(n), "D"(a0), "S"(a1), "d"(a2)
                  : "rcx", "r11", "memory");
 return r;
}
static inline void sys_debug(long v) { syscall1(1, v); }
static inline long sys_getpid(long v){ long __val = syscall1(2, v); return __val;}
static inline int sys_write(long v, void *__ptr, size_t len) {
 (void)v;
 int __val = syscall3(3, __ptr, len);
 return __val;
}
