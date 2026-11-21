#ifndef EF_H  // short form for EXCEPTION_FRAME
#define EF_H

#include <stdint.h>
struct exception_frame
{
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
  uint64_t vector, error_code;
  uint64_t rip, cs, rflags, rsp, ss;
};
_Static_assert(sizeof(struct exception_frame) == 22 * 8,
               "exception_frame layout mismatch");
#endif
