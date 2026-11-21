#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H
#include <stdint.h>
#define CPU_BIT32(n) (1u << (n))
#define CPU_BIT64(n) (1ULL << (n))
// CR0 (Intel SDM Vol. 3A, 2.5)
#define CR0_PE CPU_BIT64(0)  // Protection Enable
#define CR0_MP CPU_BIT64(1)  // Monitor Coprocessor
#define CR0_EM CPU_BIT64(2)  // x87 Emulation
#define CR0_TS CPU_BIT64(3)  // Task Switched
#define CR0_ET CPU_BIT64(4)  // Extension Type (hardwired to 1)
#define CR0_NE CPU_BIT64(5)  // Numeric Error
#define CR0_WP CPU_BIT64(16) // Write Protect
#define CR0_AM CPU_BIT64(18) // Alignment Mask
#define CR0_NW CPU_BIT64(29) // Not Write-through
#define CR0_CD CPU_BIT64(30) // Cache Disable
#define CR0_PG CPU_BIT64(31) // Paging
// CR4 (Intel SDM Vol. 3A, 2.5)
#define CR4_VME CPU_BIT64(0)
#define CR4_PVI CPU_BIT64(1)
#define CR4_TSD CPU_BIT64(2)
#define CR4_DE CPU_BIT64(3)
#define CR4_PSE CPU_BIT64(4)
#define CR4_PAE CPU_BIT64(5)
#define CR4_MCE CPU_BIT64(6)
#define CR4_PGE CPU_BIT64(7)
#define CR4_PCE CPU_BIT64(8)
#define CR4_OSFXSR CPU_BIT64(9)      // OS supports FXSAVE/FXRSTOR
#define CR4_OSXMMEXCPT CPU_BIT64(10) // OS handles #XM
#define CR4_UMIP CPU_BIT64(11)
#define CR4_LA57 CPU_BIT64(12)
#define CR4_VMXE CPU_BIT64(13)
#define CR4_SMXE CPU_BIT64(14)
#define CR4_FSGSBASE CPU_BIT64(16)
#define CR4_PCIDE CPU_BIT64(17)
#define CR4_OSXSAVE CPU_BIT64(18)
#define CR4_SMEP CPU_BIT64(20)
#define CR4_SMAP CPU_BIT64(21)
#define CR4_PKE CPU_BIT64(22)
#define CR4_CET CPU_BIT64(23)
#define CR4_PKS CPU_BIT64(24)
// XCR0 (Intel SDM Vol. 1, 13.3)
#define XCR_XCR0 0
#define XCR0_X87 CPU_BIT64(0)
#define XCR0_SSE CPU_BIT64(1)
#define XCR0_YMM CPU_BIT64(2) // AVX: upper halves of YMM
#define XCR0_OPMASK CPU_BIT64(5)
#define XCR0_ZMM_HI256 CPU_BIT64(6)
#define XCR0_HI16_ZMM CPU_BIT64(7)
#define XCR0_AVX512 (XCR0_OPMASK | XCR0_ZMM_HI256 | XCR0_HI16_ZMM)
// CPUID leaves
#define CPUID_LEAF_FEATURES 0x1
#define CPUID_LEAF_EXT_FEATURES 0x7
#define CPUID_LEAF_XSTATE 0xD
// CPUID.1:ECX
#define CPUID1_ECX_PCID CPU_BIT32(17)
#define CPUID1_ECX_X2APIC CPU_BIT32(21)
#define CPUID1_ECX_XSAVE CPU_BIT32(26)
#define CPUID1_ECX_OSXSAVE CPU_BIT32(27)
#define CPUID1_ECX_AVX CPU_BIT32(28)
// CPUID.7.0:EBX
#define CPUID7_EBX_FSGSBASE CPU_BIT32(0)
#define CPUID7_EBX_INVPCID CPU_BIT32(10)
#define CPUID7_EBX_AVX512F CPU_BIT32(16)
// API
void init_cpu_extended_states(void);
int check_cpu_xsave_environment(void);
int check_os_xsave_enabled(void);
void enable_osxsave(void);
void enable_sse(void);
void init_xcr0(void);
int enable_avx256_ymm(void);
int enable_avx512_zmm(void);
uint32_t get_xsave_size(void);
int x2apic_supported(void);
int pcid_support(void);
int invpcid_support(void);
int enable_fsbase_in_cr4(void);
#endif // CPU_FEATURES_H
