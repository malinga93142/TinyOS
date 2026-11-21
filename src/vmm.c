#include "../include/vmm.h"
#include "../include/cpu.h"
#include "../include/kstring.h"
#include "../include/list.h"
#include "../include/mm.h"
#include "../include/paging.h"
#include "../include/percpu.h"
#include "../include/printk.h"
#include <stddef.h>
#include <stdint.h>

static inline int mm_is_current(const struct mm *mm) {
 return mm->pml4_phys == (getcr3() & PAGE_ADDR_MASK);
}

static inline int vma_is_backed(const struct vma *v) {
 return v->perms & (VMA_READ | VMA_WRITE | VMA_EXEC);
}

static uint64_t vma_page_flags(const struct vma *v) {
 return PAGE_PRESENT | ((v->perms & VMA_WRITE) ? PAGE_WRITE : 0) |
        ((v->perms & VMA_EXEC) ? 0 : PAGE_NX);
}
static int mm_map_page(struct mm *mm, uint64_t va, uint64_t flags, int flush) {
 uint64_t pa = pmm_alloc_frame();
 if (!pa)
  return -1;
 kmemset(PA_TO_VA(pa), 0, PAGE_SIZE);
 if (paging_map_page_in(mm->pml4_phys, va, pa, flags, flush) == PAGING_ERR) {
  pmm_free_frame(pa);
  return -1;
 }
 return 0;
}
static void mm_unmap_range(struct mm *mm, uint64_t start, uint64_t end) {
 int flush = mm_is_current(mm);
 for (uint64_t va = start; va < end; va += PAGE_SIZE) {
  uint64_t pa = paging_unmap_page_in(mm->pml4_phys, va, flush);
  if (pa != PAGING_ERR)
   pmm_free_frame(pa);
 }
}

void mm_unmap_vma(struct mm *mm, struct vma *v) {
 if (vma_is_backed(v))
  mm_unmap_range(mm, v->start, v->end);
}

static int mm_map_vma(struct mm *mm, struct vma *v) {
 if (!vma_is_backed(v))
  return 0;
 uint64_t flags = vma_page_flags(v);
 int flush = mm_is_current(mm);
 uint64_t va;
 for (va = v->start; va < v->end; va += PAGE_SIZE) {
  // uint64_t pa = pmm_alloc_frame();
  // if (!pa)
  //  break;
  // kmemset(PA_TO_VA(pa), 0, PAGE_SIZE);
  // if (paging_map_page_in(mm->pml4_phys, va, pa, flags, flush) == PAGING_ERR) {
  //  pmm_free_frame(pa);
  //  break;
  // }
  // for (va = v->start; va < v->end; va += PAGE_SIZE)
  if (mm_map_page(mm, va, flags, flush) < 0)
   break;
 }
 if (va < v->end) {
  mm_unmap_range(mm, v->start, va);
  return -1;
 }
 return 0;
}

static void mm_drop_vma(struct mm *mm, struct vma *v) {
 uint64_t fl = spin_lock_irqsave(&mm->mm_lock);
 list_del(&v->list);
 spin_unlock_irqrestore(&mm->mm_lock, fl);
 kfree(v);
}

struct mm *mm_create(void) {
 struct mm *mm = kzalloc(sizeof *mm);
 if (!mm)
  return NULL;
 uint64_t pa = pmm_alloc_frame();
 if (!pa) {
  kfree(mm);
  return NULL;
 }
 uint64_t *pml4 = PA_TO_VA(pa);
 uint64_t *kernel_pml4 = PA_TO_VA(getcr3() & PAGE_ADDR_MASK);
 kmemset(pml4, 0, PAGE_SIZE);
 pml4[511] = kernel_pml4[511];
 mm->pml4_phys = pa;
 list_init(&mm->vma_list);
 spinlock_init(&mm->mm_lock);
 return mm;
}

struct vma *mm_add_vma(struct mm *mm, uint64_t start, uint64_t end, int perms,
                       enum vma_kind kind) {
 if (start >= end || ((start | end) & (PAGE_SIZE - 1)))
  return NULL;
 struct vma *v = kzalloc(sizeof *v);
 if (!v)
  return NULL;
 v->start = start;
 v->end = end;
 v->perms = perms;
 v->kind = kind;
 uint64_t fl = spin_lock_irqsave(&mm->mm_lock);
 struct vma *it, *hit = NULL;
 list_for_each_entry(it, &mm->vma_list, list) {
  if (it->end > start) {
   hit = it;
   break;
  }
 }
 int overlap = hit && hit->start < end;
 if (!overlap)
  list_add_tail(&v->list, hit ? &hit->list : &mm->vma_list);
 spin_unlock_irqrestore(&mm->mm_lock, fl);
 if (overlap) {
  kfree(v);
  return NULL;
 }
 return v;
}

int mm_mmap(struct mm *mm, uint64_t start, uint64_t end, int perms,
            enum vma_kind kind) {
 struct vma *v = mm_add_vma(mm, start, end, perms, kind);
 if (!v)
  return -1;
 if (mm_map_vma(mm, v) < 0) {
  mm_drop_vma(mm, v);
  return -1;
 }
 return 0;
}

/* mm must NOT be loaded in CR3 on any CPU when this runs. */
void mm_destroy(struct mm *mm) {
 if (!mm)
  return;
 while (!list_empty(&mm->vma_list)) {
  struct vma *v = list_entry(mm->vma_list.next, struct vma, list);
  list_del(&v->list);
  mm_unmap_vma(mm, v);
  kfree(v);
 }
 pml4_free(mm->pml4_phys & PAGE_ADDR_MASK);
 kfree(mm);
}

static uint64_t mm_virt_to_phys(struct mm *mm, uint64_t va) {
	uint64_t t = mm->pml4_phys;
	for (int s = 39; s >= 12; s -= 9) {
		uint64_t e = ((uint64_t *)PA_TO_VA(t))[(va >> s) & 0x1FF];
		if (!(e & PAGE_PRESENT)) {
			return 0;
		}
		t = e & PAGE_ADDR_MASK;
	}
	return t | (va & 0xFFF);
}

void mm_copy_to(struct mm* mm, uint64_t va, const void *src, uint64_t n) {
	while (n) {
		uint64_t pa = mm_virt_to_phys(mm, va);
		uint64_t c = PAGE_SIZE - (va & 0xFFF);
		if (!pa) return;
		if (c > n) c = n;
		kmemcpy(PA_TO_VA(pa), src, c);
		va += c;
		src = (const uint8_t *)src + c;
		n -= c;
	}
}
static struct vma *mm_find_vma(struct mm *mm, uint64_t addr) {
 struct vma *v, *r = NULL;
 uint64_t fl = spin_lock_irqsave(&mm->mm_lock);
 list_for_each_entry(v, &mm->vma_list, list) {
  if (addr < v->start)
   break;
  if (addr < v->end) {
   r = v;
   break;
  }
 }
 spin_unlock_irqrestore(&mm->mm_lock, fl);
 return r;
}

int mm_handle_fault(struct mm *mm, uint64_t addr, uint64_t err) {
 struct vma *v = mm_find_vma(mm, addr);
 if (!v || !vma_is_backed(v) || (err & 1))
  return -1;
 if (((err & 2) && !(v->perms & VMA_WRITE)) ||
     ((err & 16) && !(v->perms & VMA_EXEC)))
  return -1;
 return mm_map_page(mm, addr & ~(uint64_t)(PAGE_SIZE - 1), vma_page_flags(v),
                    mm_is_current(mm));
}
