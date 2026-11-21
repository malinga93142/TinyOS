#include "../include/cpu_features.h"
#include "../include/cpu.h"
#include "../include/cpu_features.h"
#include "../include/printk.h"

// primitives
static inline void cpu_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax,
                             uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
  __asm__ volatile("cpuid"
                   : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                   : "a"(leaf), "c"(subleaf));
}
static inline uint32_t cpuid1_ecx(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(CPUID_LEAF_FEATURES, 0, &a, &b, &c, &d);
  return c;
}
static inline uint32_t cpuid7_ebx(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(CPUID_LEAF_EXT_FEATURES, 0, &a, &b, &c, &d);
  return b;
}
static inline uint64_t cpu_xgetbv(uint32_t index) {
  uint32_t lo, hi;
  __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(index));
  return ((uint64_t)hi << 32) | lo;
}
static inline void cpu_xsetbv(uint32_t index, uint64_t value) {
  __asm__ volatile("xsetbv"
                   :
                   : "c"(index), "a"((uint32_t)value),
                     "d"((uint32_t)(value >> 32)));
}
// XCR0 bits the CPU allows the OS to enable: CPUID.(0DH,0):EDX:EAX
static uint64_t xcr0_supported_mask(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(CPUID_LEAF_XSTATE, 0, &a, &b, &c, &d);
  return ((uint64_t)d << 32) | a;
}
// Read-modify-write XCR0, refusing bits the CPU does not support (XSETBV would
// #GP).
static int xcr0_enable(uint64_t bits) {
  if ((xcr0_supported_mask() & bits) != bits)
    return -1;
  cpu_xsetbv(XCR_XCR0, cpu_xgetbv(XCR_XCR0) | bits);
  return 0;
}
// XSAVE / SIMD state
int check_cpu_xsave_environment(void) {
  return (cpuid1_ecx() & CPUID1_ECX_XSAVE) != 0;
}
// CPUID.1:ECX.OSXSAVE mirrors CR4.OSXSAVE
int check_os_xsave_enabled(void) {
  return (cpuid1_ecx() & CPUID1_ECX_OSXSAVE) != 0;
}
void enable_osxsave(void) { setcr4(getcr4() | CR4_OSXSAVE); }
// Size of the (standard-format) XSAVE area for features currently in XCR0
uint32_t get_xsave_size(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(CPUID_LEAF_XSTATE, 0, &a, &b, &c, &d);
  return b;
}
// Baseline SSE: independent of XSAVE/XCR0
void enable_sse(void) {
  setcr0((getcr0() & ~CR0_EM) | CR0_MP);
  setcr4(getcr4() | CR4_OSFXSR | CR4_OSXMMEXCPT);
}
// Minimum legal XCR0: x87 must always be set, SSE must be set before AVX
void init_xcr0(void) { cpu_xsetbv(XCR_XCR0, XCR0_X87 | XCR0_SSE); }
int enable_avx256_ymm(void) {
  if (!(cpuid1_ecx() & CPUID1_ECX_AVX)) {
    kprintf("[KERNEL] AVX is not supported\n");
    return -1;
  }
  return xcr0_enable(XCR0_YMM);
}
int enable_avx512_zmm(void) {
  if (!(cpuid7_ebx() & CPUID7_EBX_AVX512F)) {
    kprintf("[KERNEL] AVX512F is not supported\n");
    return -1;
  }
  // AVX-512 state requires the AVX (YMM) bit to be set already
  if (!(cpu_xgetbv(XCR_XCR0) & XCR0_YMM)) {
    kprintf("[KERNEL] AVX512 requires AVX to be enabled first\n");
    return -1;
  }
  kprintf("[KERNEL] AVX512F is supported\n");
  return xcr0_enable(XCR0_AVX512); // bits 5-7 are all-or-none
}
void init_cpu_extended_states(void) {
  enable_sse();
  if (!check_cpu_xsave_environment()) {
    kprintf("[KERNEL] XSAVE not supported, falling back to FXSAVE\n");
    return;
  }
  enable_osxsave();
  if (!check_os_xsave_enabled()) {
    kprintf("[KERNEL] CR4.OSXSAVE did not take effect\n");
    return;
  }
  kprintf("[KERNEL] OSXSAVE enabled. CR4 = %x\n", (uint32_t)getcr4());
  init_xcr0();
  if (enable_avx256_ymm() == 0)
    enable_avx512_zmm();
  // only valid once XCR0 is final
  uint32_t xsave_size = get_xsave_size();
  kprintf("[KERNEL] XCR0 = %x, XSAVE size = %d bytes\n",
          (uint32_t)cpu_xgetbv(XCR_XCR0), xsave_size);
}
// other CPU features
int x2apic_supported(void) { return (cpuid1_ecx() & CPUID1_ECX_X2APIC) != 0; }

int pcid_support(void) {
  uint32_t ecx = cpuid1_ecx();
  kprintf("[KERNEL] ECX = %x\n", ecx);
  if (!(ecx & CPUID1_ECX_PCID))
    return -1;
  kprintf("[KERNEL] PCID is supported. Enabling CR4.PCIDE\n");
  kprintf("[KERNEL] CR4 = %x\n", (uint32_t)getcr4());
  setcr4(getcr4() | CR4_PCIDE);
  kprintf("[KERNEL] CR4 = %x\n", (uint32_t)getcr4());
  return 0;
}
int invpcid_support(void) {
  uint32_t ebx = cpuid7_ebx();
  kprintf("[KERNEL] EBX = %x\n", ebx);
  if (!(ebx & CPUID7_EBX_INVPCID))
    return -1;
  kprintf("[KERNEL] INVPCID is supported.\n");
  return 0;
}
int enable_fsbase_in_cr4(void) {
  if (!(cpuid7_ebx() & CPUID7_EBX_FSGSBASE))
    return -1;
  setcr4(getcr4() | CR4_FSGSBASE);
  return 0;
}
