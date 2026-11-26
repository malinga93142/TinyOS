#include "../include/paging.h"
#include "../include/cpu.h"
#include "../include/kstring.h"
#include "../include/mm.h"
#include "../include/pmm.h"
#include "../include/printk.h"
#include <stdatomic.h>
#include <stdint.h>
typedef uint64_t pte_t;

// Common helpers
static inline void invalidate(uint64_t va) {
  __asm__ volatile("invlpg (%0)" ::"r"(va) : "memory");
}
static inline int is_canonical(uint64_t va) {
  return ((int64_t)(va << 16) >> 16) == (int64_t)va;
}
static inline void halt_forever(void) {
  for (;;)
    asm volatile("hlt");
}
static inline void require_canonical(uint64_t va) {
  if (!is_canonical(va))
    panic("[KERNEL] Non-Canonical Virtual Address: %x\n", va);
}
// Physical address -> pointer to the page table it holds.
static inline uint64_t *tbl_va(uint64_t phys_or_entry) {
  return (uint64_t *)PA_TO_VA(phys_or_entry & PAGE_ADDR_MASK);
}
// Allocate one zeroed physical frame. Returns 0 on failure.
static uint64_t alloc_zeroed_frame(void) {
  uint64_t pa = pmm_alloc_frame();
  // kprintf("%x\n", pa);
  if (!pa)
    return 0;
  kmemset(PA_TO_VA(pa), 0, PAGE_SIZE);
  // kprintf("zeroed frame: %x\n", pa);
  return pa;
}

// Descend one level. If the entry is absent and `create` is set, allocate
// a new table. Returns NULL on OOM, on a huge-page entry, or when absent
// and !create.
static uint64_t *table_next(uint64_t *tbl, uint64_t idx, int create) {
  uint64_t e = tbl[idx];
  if (!(e & PAGE_PRESENT)) {
    if (!create)
      return NULL;
    uint64_t pa = alloc_zeroed_frame();
    if (!pa)
      return NULL;
    e = tbl[idx] = MAKE_ENTRY(pa, PAGE_PRESENT | PAGE_WRITE);
  } else if (e & PAGE_HUGE) {
    return NULL;
  }
  return tbl_va(e);
}

// Walk PML4 -> PDPT -> PD and return the PT (4K level) for va.
static uint64_t *walk_to_pt(uint64_t pml4_phys, uint64_t va, int create) {
 uint64_t *t = tbl_va(pml4_phys);
 // kprintf("%x\n", t);
 t = table_next(t, PML4_INDEX(va), create);
 if (!t) { kprintf("PML4NULL\n"); return NULL; }
 t = table_next(t, PDPT_INDEX(va), create);
 if (!t) { kprintf("PDPTNULL\n"); return NULL; }
 t = table_next(t, PD_INDEX(va), create);
 if (!t) { kprintf("PDNULL\n"); return NULL; }
 return t;
}
static inline uint64_t page_offset_mask(int level) {
  return level == 3   ? PAGE_OFFSET_MASK_1G
         : level == 2 ? PAGE_OFFSET_MASK_2M
                      : PAGE_OFFSET_MASK_4K;
}
// Public API
// Read-only walk. Sets *level to 3 (1G), 2 (2M) or 1 (4K).
pte_t *paging_walk(uint64_t pml4_phys, uint64_t va, int *level) {
  uint64_t *t = tbl_va(pml4_phys);
  uint64_t idx[4] = {PML4_INDEX(va), PDPT_INDEX(va), PD_INDEX(va),
                     PT_INDEX(va)};
  for (int i = 0; i < 4; i++) {
    pte_t *p = &t[idx[i]];
    if (!(*p & PAGE_PRESENT))
      return NULL;
    if (i == 3) {
      *level = 1;
      return p;
    }
    if (i > 0 && (*p & PAGE_HUGE)) { // PDPT -> 3, PD -> 2
      *level = 4 - i;
      return p;
    }
    t = tbl_va(*p);
  }
  return NULL;
}

pte_t *paging_walk_create(uint64_t pml4_phys, uint64_t va) {
  uint64_t *pt = walk_to_pt(pml4_phys, va, 1);
  return pt ? &pt[PT_INDEX(va)] : NULL;
}
uint64_t paging_map_page_in(uint64_t root_phys, uint64_t va, uint64_t pa,
                            uint64_t flags, int flush) {
 require_canonical(va);
 uint64_t *pt = walk_to_pt(root_phys, va, 1);
 if (!pt) {
  kprintf("%lx\n", pt);
  return PAGING_ERR;
 }
 if (pt[PT_INDEX(va)] & PAGE_PRESENT)
  return PAGING_ERR;
 if (va < 0x0000800000000000) {
  uint64_t t = root_phys;
  for (int s = 39; s >= 21; s -= 9) {
   uint64_t *e = &((uint64_t *)PA_TO_VA(t))[(va >> s) & 0x1FF];
   *e |= PAGE_USER;
   t = *e & PAGE_ADDR_MASK;
  }
  flags |= PAGE_USER;
 }
 pt[PT_INDEX(va)] = MAKE_ENTRY(pa, flags | PAGE_PRESENT);
 if (flush)
  invalidate(va);
 return va;
}
/*
 * maps a page to assigned physical address
 * va virtual address
 * pa physical address
 * flags flags
 */
uint64_t paging_map_page(uint64_t va, uint64_t pa, uint64_t flags) {
 // kprintf("%lx\n", va);
 return paging_map_page_in(getcr3() & PAGE_ADDR_MASK, va, pa, flags, 1);
}

uint64_t paging_unmap_page_in(uint64_t root_phys, uint64_t va, int flush) {
  require_canonical(va);
  uint64_t *pt = walk_to_pt(root_phys, va, 0);
  if (!pt)
    return PAGING_ERR;
  uint64_t idx = PT_INDEX(va);
  if (!(pt[idx] & PAGE_PRESENT))
    return PAGING_ERR;
  uint64_t pa = ENTRY_ADDR(pt[idx]);
  pt[idx] = 0;
  if (flush)
    invalidate(va);
  return pa;
}

uint64_t paging_unmap_page(uint64_t va) {
  uint64_t pa = paging_unmap_page_in(getcr3() & PAGE_ADDR_MASK, va, 1);
  if (pa == PAGING_ERR)
    return PAGING_ERR;
  pmm_free_frame(pa);
  return pa;
}

void paging_init(uint64_t kernel_phys_start, uint64_t kernel_phys_end) {
  uint64_t start = PAGE_ALIGN_DOWN(kernel_phys_start);
  uint64_t end = PAGE_ALIGN_UP(kernel_phys_end);
  for (uint64_t pa = start; pa < end; pa += PAGE_SIZE) {
    uint64_t va = KERNEL_BASE + pa;
    if (paging_virt_to_phys(va) == pa)
      continue;
    uint64_t ret = paging_map_page(KERNEL_BASE + pa, pa, PAGE_WRITE);
    if (ret == PAGING_ERR) {
      kprintf("failed mapping %lx, ret=%x\n", pa, ret);
      halt_forever();
    }
  }
  kprintf("[VMM] KERNEL MAPPED %x - %x\n", start, end);
}

// Demand-paging: back addr with a fresh zeroed frame.
void page_fault_map(uint64_t addr) {
  addr = PAGE_ALIGN_DOWN(addr);
  uint64_t *pt = walk_to_pt(getcr3(), addr, 1);
  uint64_t frame = pt ? alloc_zeroed_frame() : 0;
  if (!frame)
    halt_forever();
  pt[PT_INDEX(addr)] = MAKE_ENTRY(frame, PAGE_PRESENT | PAGE_WRITE);
  invalidate(addr);
}

uint64_t paging_virt_to_phys_in(uint64_t pml4_phys, uint64_t va) {
  int level;
  pte_t *pte = paging_walk(pml4_phys, va, &level);
  if (!pte)
    return PAGING_ERR;
  uint64_t m = page_offset_mask(level);
  return (*pte & PAGE_ADDR_MASK & ~m) | (va & m);
}
uint64_t paging_virt_to_phys(uint64_t va) {
  return paging_virt_to_phys_in(getcr3(), va);
}
void unmap_lower_half(void) {
  uint64_t cr3 = getcr3();
  tbl_va(cr3)[0] = 0;
  asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
  kprintf("[KERNEL] PML4[0] UNMAPPED.\n");
}

int aspace_map(uint64_t pml4_pa, uint64_t va, uint64_t pa, uint64_t flags) {
  if (PML4_INDEX(va) == 511) // kernel half is off limits
    return -1;
  uint64_t *pt = walk_to_pt(pml4_pa, va, 1);
  if (!pt)
    return -1;
  pt[PT_INDEX(va)] = (pa & PAGE_ADDR_MASK) | flags | PAGE_PRESENT;
  if ((getcr3() & PAGE_ADDR_MASK) == pml4_pa)
    invalidate(va);
  return 0;
}

static void free_huge(uint64_t pa, uint64_t frames) {
  for (uint64_t k = 0; k < frames; k++) {
    pmm_free_frame(pa + PAGE_SIZE * k);
  }
}
static void pt_free(uint64_t pt_pa) {
  pt_pa &= PAGE_ADDR_MASK;
  uint64_t *pt = (uint64_t *)PA_TO_VA(pt_pa);
  for (int i = 0; i < 512; i++) {
    uint64_t e = pt[i];
    if (!(e & PAGE_PRESENT))
      continue;
    pmm_free_frame(e & PAGE_ADDR_MASK);
    pt[i] = 0;
  }
  pmm_free_frame(pt_pa);
}
static void pd_free(uint64_t pd_pa) {
  pd_pa &= PAGE_ADDR_MASK;
  uint64_t *pd = (uint64_t *)PA_TO_VA(pd_pa);
  for (int i = 0; i < 512; i++) {
    uint64_t e = pd[i];
    if (!(e & PAGE_PRESENT))
      continue;
    uint64_t child = e & PAGE_ADDR_MASK;
    if (e & PAGE_HUGE)
      free_huge(child, 512);
    else
      pt_free(child);
    pd[i] = 0;
  }
  pmm_free_frame(pd_pa);
}
static void pdpt_free(uint64_t pdpt_pa) {
  pdpt_pa &= PAGE_ADDR_MASK;
  uint64_t *pdpt = (uint64_t *)PA_TO_VA(pdpt_pa);
  for (int i = 0; i < 512; i++) {
    uint64_t e = pdpt[i];
    if (!(e & PAGE_PRESENT))
      continue;
    uint64_t child = e & PAGE_ADDR_MASK;
    if (e & PAGE_HUGE)
      free_huge(child, 512ULL * 512);
    else
      pd_free(child);
    pdpt[i] = 0;
  }
  pmm_free_frame(pdpt_pa);
}

static int is_current_kernel_aspace(uint64_t pml4) {
  pml4 &= PAGE_ADDR_MASK;
  return getcr3() == pml4 ? 1 : 0;
}
void pml4_free(uint64_t pml4_pa) {
  pml4_pa &= PAGE_ADDR_MASK;
  if (is_current_kernel_aspace(pml4_pa) == 1) {
    panic("[PANIC] kernel va space can't freed");
  }
  uint64_t *pml4 = (uint64_t *)PA_TO_VA(pml4_pa);

  for (int i = 0; i < 511; i++) {
    uint64_t e = pml4[i];
    if (!(e & PAGE_PRESENT))
      continue;
    pdpt_free(e & PAGE_ADDR_MASK);
    pml4[i] = 0;
  }
  pmm_free_frame(pml4_pa);
}
