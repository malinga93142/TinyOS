#ifndef VMM_H
#define VMM_H
#include "list.h"
#include <stdint.h>

// PML4[511], PDPT[0]: image + direct map
#define KERNEL_VIRT_BASE 0xFFFFFF8000000000ULL
#define KERNEL_VIRT_BASE_OLD 0xFFFFFFFF80000000
// PDPT[0..63] reserved for RAM
#define DIRECTMAP_END (KERNEL_VIRT_BASE + (64ULL << 30))

// 0xFFFFFF9000000000 PDPT[64] to PDPT[127]
#define VMALLOC_BASE (KERNEL_VIRT_BASE + (64ULL << 30))
#define VMALLOC_END (KERNEL_VIRT_BASE + (128ULL << 30))

// 0xFFFFFFB000000000 PDPT[192] to PDPT[255]
#define KSTACK_BASE (KERNEL_VIRT_BASE + (192ULL << 30))
#define KSTACK_END (KERNEL_VIRT_BASE + (256ULL << 30))

#define IS_DIRECTMAP_ADDR(va) ((va) >= KERNEL_VIRT_BASE && (va) < DIRECTMAP_END)
#define VMALLOC_GAP_PAGES 1

#define IS_VMALLOC_ADDR(va) ((va) >= VMALLOC_BASE && (va) < VMALLOC_END)
#define PAGES_FOR(bytes) (((bytes) + PAGE_SIZE - 1) / PAGE_SIZE)
#define VA_UNMAPPED (~0ULL)

struct vma {
  uint64_t start;
  uint64_t end;
  int perms;
  int kind;
  // struct vma *vmas;
  struct list_head list;
};

#define VMA_READ (1 << 0)
#define VMA_WRITE (1 << 1)
#define VMA_EXEC (1 << 2)

enum vma_kind {
  VMA_KIND_TEXT,
  VMA_KIND_RODATA,
  VMA_KIND_DATA,
  VMA_KIND_HEAP,
  VMA_KIND_STACK,
};
#define VMA_HEAP 1
#define VMA_STK 2
// function signatures
void *vmalloc(uint64_t npages);
void *vzalloc(uint64_t npages);
void vfree(void *ptr);
void *kmalloc(uint64_t bytes);
void *kzalloc(uint64_t bytes);
void kfree(void *ptr);

#endif // VMM_H
