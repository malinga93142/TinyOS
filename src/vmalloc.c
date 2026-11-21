#include "../include/vmm.h"
#include "../include/kstring.h"
#include "../include/mm.h"
#include "../include/paging.h"
#include "../include/percpu.h"
#include "../include/printk.h"
#include <stdatomic.h>
#include <stddef.h>
static _Atomic uint64_t vm_next = VMALLOC_BASE;
void *vmalloc(uint64_t npages) {
  if (!npages || npages >= (VMALLOC_END - VMALLOC_BASE) / PAGESIZE)
    return NULL;
  uint64_t size = (npages + VMALLOC_GAP_PAGES) * PAGE_SIZE;
  uint64_t base =
      atomic_fetch_add_explicit(&vm_next, size, memory_order_relaxed);
  if (base + size > VMALLOC_END)
    return NULL;

  uint64_t i;
  for (i = 0; i < npages; i++) {
    uint64_t pa = pmm_alloc_frame();
    if (!pa)
      break;
    uint64_t r = paging_map_page(base + i * PAGE_SIZE, pa, PAGE_WRITE);
    if (r == PAGING_ERR) {
      // frame_put(pa);
      pmm_free_frame(pa);
      kprintf("[CPU %d]MAPPING FAILED\n", cpu_id());
      break;
    }
  }
  if (i < npages) {
    while (i--)
      vfree((void *)((char *)base + i * PAGE_SIZE));
    return NULL;
  }
  return (void *)base;
}

void *vzalloc(uint64_t npages) {
  void *p = vmalloc(npages);
  if (p)
    kmemset(p, 0, npages * PAGE_SIZE);
  return p;
}

void vfree(void *ptr) {
  uint64_t va = (uint64_t)ptr;
  if (!va)
    return;
  if (!IS_VMALLOC_ADDR(va) || (va & PAGE_OFFSET_MASK_4K))
    panic("[VMALLOC] bad free %lx\n", va);
  if (va != VMALLOC_BASE && paging_virt_to_phys(va - PAGE_SIZE) != VA_UNMAPPED)
    panic("[VMALLOC] not an allocation start %lx\n", va);

  uint64_t pa = paging_unmap_page(va);
  if (pa == PAGING_ERR)
    panic("[VMALLOC] double free %lx\n", va);

  for (;;) {
    // frame_put(pa);
    // pmm_free_frame(pa);
    va += PAGE_SIZE;
    if (!IS_VMALLOC_ADDR(va))
      break;
    pa = paging_unmap_page(va);
    if (pa == PAGING_ERR)
      break;
  }
}
