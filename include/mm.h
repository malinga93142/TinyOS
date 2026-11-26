#ifndef MM_H
#define MM_H
#include "list.h"
#include "spinlock.h"
#include "vmm.h"
#include <stdint.h>
// Paging constants
#define PAGE_SIZE 0x1000ULL
#define PAGE_SHIFT 12
//  Virtual address layout
//  Higher-half kernel:
// 0xffffff8000000000ULL - 0xffffffffffffffff
#define KERNEL_BASE 0xFFFFFF8000000000ULL
#define KERNEL_BASE_OLD 0xFFFFFFFF80000000ULL
#define KERNEL_TOP  0xFFFFFFFFFFFFFFFFULL
#define KERNEL_IO_BASE 0xFFFFFFF800000000
#define PERCPU_BASE (KERNEL_BASE + 0x000000F000000000UL)
//  Physical <-> Higher-half virtual conversion  Requires kernel direct mapping
//  of physical memory.
#define PA_TO_VA(pa) ((void *)((uint64_t)(pa) + KERNEL_BASE))
#define PA_TO_VA_PERCPU(pa) ((void *)((uint64_t)(pa) + PERCPU_BASE))
#define VA_TO_PA(va) ((uintptr_t)(va) - KERNEL_BASE)
#define PA_TO_VA_MMIO(pa) ((void *)((uint64_t)(pa) + KERNEL_IO_BASE))
#define VA_TO_PA_MMIO(va) ((uint64_t)(va) - KERNEL_IO_BASE)
/* Alignment helpers */
#define PAGE_ALIGN_DOWN(addr) ((uintptr_t)(addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr)                                                    \
  (((uintptr_t)(addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

#define PML4_INDEX(va) (((uintptr_t)(va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((uintptr_t)(va) >> 30) & 0x1FF)
#define PD_INDEX(va) (((uintptr_t)(va) >> 21) & 0x1FF)
#define PT_INDEX(va) (((uintptr_t)(va) >> 12) & 0x1FF)
#define PAGE_OFFSET(va) ((uintptr_t)(va) & 0xFFF)
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_PWT (1ULL << 3)
#define PAGE_PCD (1ULL << 4)
#define PAGE_ACCESSED (1ULL << 5)
#define PAGE_DIRTY (1ULL << 6)
#define PAGE_HUGE (1ULL << 7)
#define PAGE_GLOBAL (1ULL << 8)
#define PAGE_NX (1ULL << 63)
#define PAGE_SIZE_4K 0x1000ULL
#define PAGE_SIZE_2M 0x200000ULL
#define PAGE_SIZE_1G 0x40000000ULL
#define PAGE_OFFSET_MASK_4K (PAGE_SIZE_4K - 1) // 0xFFF
#define PAGE_OFFSET_MASK_2M (PAGE_SIZE_2M - 1) // 0x1FFFFF
#define PAGE_OFFSET_MASK_1G (PAGE_SIZE_1G - 1) // 0x3FFFFFFF
// Physical address stored inside page entries
#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define MAKE_ENTRY(pa, flags)                                                  \
  (((uint64_t)(pa) & PAGE_ADDR_MASK) | (uint64_t)(flags))
#define ENTRY_ADDR(entry) ((uint64_t)(entry) & PAGE_ADDR_MASK)

#define UHEAP_PAGES 5
#define UHEAP_START  0x55555555A000ULL
#define UHEAP_END    (UHEAP_START + (PAGE_SIZE * UHEAP_PAGES))

#define USTACK_PAGES 4
#define USTACK_START 0x7FFFFFFFB000ULL
#define USTACK_END   (USTACK_START + (PAGE_SIZE * USTACK_PAGES))
struct mm {
  uint64_t pml4_phys;
  // struct vma *vma;
  struct list_head vma_list;
  spinlock_t mm_lock;
};
struct vma *mm_add_vma(struct mm *mm, uint64_t start, uint64_t end, int perms,
                       enum vma_kind kind);
struct mm *mm_create(void);
void mm_destroy(struct mm *mm);
int mm_mmap(struct mm *mm, uint64_t start, uint64_t end, int perms,
            enum vma_kind kind);
void mm_copy_to(struct mm* mm, uint64_t va, const void *src, uint64_t n);
int mm_handle_fault(struct mm *mm, uint64_t addr, uint64_t err);
#endif
