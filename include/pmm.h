#ifndef PMM_H
#define PMM_H
#include <stdint.h>
// #include "paging.h"
#define PAGESIZE 4096
#define RAM_SIZE (512 * 2ULL * 1024 * 1024)
#define PAGECOUNT (RAM_SIZE / PAGESIZE)
struct phys_info {
  uint64_t base;
  uint64_t len;
};
extern struct phys_info __pinfo[4];

/* PMM */
// uint32_t pmm_alloc_frame_below(uint32_t);
uint64_t pmm_alloc_frame_below(uint64_t range);
/* Call once at boot, after read_mmapable_range has populated.
 * Builds the frame bitmap: everything defaults to "used", then usable
 * e820 regions are punched out as free. */
void pmm_init();

/* Mark [base, base+len) as used (rounds outward to whole frames).
 * Use this to reserve the kernel image, the multiboot info struct,
 * the mmap buffer itself, etc. before handing out frames. */
void pmm_reserve_region(uint64_t base, uint64_t len);

/* Returns the physical address of a free 4KB frame and marks it used,
 * or 0 if out of memory. 0 is never a valid returned frame because
 * frame 0 is always reserved (see pmm_init). */
uint64_t pmm_alloc_frame(void);
uint64_t pmm_alloc_frame_heap(void);
/* Marks a previously-allocated frame free again. addr must be
 * page-aligned. */
void pmm_free_frame(uint64_t addr);

/* Stats, mainly for debugging via uart */
uint64_t pmm_total_frames(void);
uint64_t pmm_free_frames(void);
#define FRAME_RESERVED 0x1
void frame_get(uint64_t pa);
void frame_put(uint64_t pa);
uint64_t frame_refs(uint64_t pa);
void print_frame_count(uint64_t);
#endif
