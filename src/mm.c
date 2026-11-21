// #include "../include/mm.h"
// #include "../include/cpu.h"
// #include "../include/kmalloc.h"
// #include "../include/kstring.h"
// #include "../include/paging.h"
// #include "../include/pmm.h"
// #include <stddef.h>

// all these exported to vmm.c
// struct mm *mm_create(void) {
//   struct mm *mm = kzalloc(sizeof(struct mm));
//   if (!mm)
//     return NULL;
//   uint64_t pa = pmm_alloc_frame();
//   if (!pa) {
//     kfree(mm);
//     return NULL;
//   }
//   uint64_t *pml4 = (uint64_t *)PA_TO_VA(pa);
//   kmemset(pml4, 0, 4096);
//   uint64_t *kernel_pml4 = (uint64_t *)PA_TO_VA(getcr3() & PAGE_ADDR_MASK);
//   pml4[511] = kernel_pml4[511];
//   mm->pml4_phys = pa;
//   spinlock_init(&mm->mm_lock);
//   return mm;
// }

// returns -1 on failures
// returns 0 on success.
// int mm_add_vma(struct mm *mm, uint64_t start, uint64_t end, int perms,
//                enum vma_kind kind) {
//   if (start >= end || (start | end) & (PAGE_SIZE - 1)) {
//     return -1;
//   }
//   struct vma *v = kzalloc(sizeof *v);
//   if (!v) {
//     return -1;
//   }
//   *v = (struct vma){
//       .start = start, .end = end, .perms = perms, .kind = kind, .vmas =
//       NULL};
//
//   uint64_t fl = spin_lock_irqsave(&mm->mm_lock);
//   struct vma **pp = &mm->vma;
//   while (*pp && (*pp)->end <= start) {
//     pp = &(*pp)->vmas;
//   }
//   if (*pp && (*pp)->start < end) {
//     spin_unlock_irqrestore(&mm->mm_lock, fl);
//     kfree(v);
//     return -1;
//   }
//   v->vmas = *pp;
//   *pp = v;
//   spin_unlock_irqrestore(&mm->mm_lock, fl);
//   return 0;
// }
