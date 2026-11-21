#include "../include/pmm.h"
#include "../include/mb.h"
#include "../include/mm.h"
#include "../include/printk.h"
#include "../include/spinlock.h"
#include <stdatomic.h>
#include <stdint.h>
// Returned by every allocator on failure. Physical address 0 is always
// reserved(see pmm_init), so it can never be a valid frame.
#ifndef PMM_NO_FRAME
#define PMM_NO_FRAME 0ULL
#endif
#define NO_SLOT (-1)
#define MAX_REGIONS 4

struct phys_info __pinfo[MAX_REGIONS];
static uint64_t pagecount = 0;
static uint64_t ramsize = 0;
static int count = 0;
extern char kernel_start[];
extern char kernel_end[];

static uint8_t bitmap[PAGECOUNT];
static atomic_uint free_count = 0;

struct frame {
  _Atomic uint32_t refcount;
  uint16_t flags;
  uint16_t future;
};
_Static_assert(sizeof(struct frame) == 8, "frame size");
static struct frame frame_table[PAGECOUNT];
static spinlock_t pmm_lock = SPINLOCK_INIT;
// next-fit hint, avoids rescanning from 0 every time
static uint32_t last_alloc_frame = 0;
// Common helpers
static inline void bit_set(uint64_t frame) {
  bitmap[frame >> 3] |= (uint8_t)(1u << (frame & 7));
}
static inline void bit_clear(uint64_t frame) {
  bitmap[frame >> 3] &= (uint8_t)~(1u << (frame & 7));
}
static inline int bit_test(uint64_t frame) {
  return bitmap[frame >> 3] & (uint8_t)(1u << (frame & 7));
}

static inline void fc_inc(void) {
  atomic_fetch_add_explicit(&free_count, 1, memory_order_relaxed);
}
static inline void fc_dec(void) {
  atomic_fetch_sub_explicit(&free_count, 1, memory_order_relaxed);
}
// A frame number that refers to real, non-zero RAM we track.
static inline int pfn_valid(uint64_t pfn) {
  return pfn != 0 && pfn < PAGECOUNT;
}
// First free frame in [from, to), or NO_SLOT.
static int64_t scan_free(uint64_t from, uint64_t to) {
  for (uint64_t f = from; f < to; f++)
    if (!bit_test(f))
      return (int64_t)f;
  return NO_SLOT;
}

// Find and claim a free frame in [from, to). Caller holds pmm_lock
// (or is single - threaded early boot)
static uint64_t claim_free(uint64_t from, uint64_t to) {
  int64_t f = scan_free(from, to);
  if (f == NO_SLOT)
    return PMM_NO_FRAME;
  bit_set(f);
  fc_dec();
  return (uint64_t)f * PAGESIZE;
}
// boot-time setup
void parse_mmap(struct multiboot_tag_mmap *mmap) {
  uint8_t *ptr = (uint8_t *)mmap->entries;
  int i = 0;
  while (ptr < (uint8_t *)mmap + mmap->size) {
    struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)ptr;
    kprintf("[PMM] base=%lx length=%lx type=%lx\n", entry->addr, entry->len,
            entry->type);
    if (entry->type == 1) {
      if (i >= MAX_REGIONS) {
        kprintf("[PMM] too many usable regions, ignoring the rest\n");
        break;
      }
      __pinfo[i].base = entry->addr;
      __pinfo[i].len = entry->len;
      ramsize += entry->len;
      i++;
    }
    ptr += mmap->entry_size;
  }
  count = i;
  pagecount = ramsize / PAGESIZE;
  kprintf("[PMM] Total Usable Memory: %lu MB (%lu Bytes)\n",
          ramsize / (1024 * 1024), ramsize);
  kprintf("[PMM] Total Frames: %lu (4KB Pages)\n", pagecount);
}
void pmm_reserve_region(uint64_t base, uint64_t len) {
  if (len == 0)
    return;
  uint64_t start_frame = base / PAGESIZE;
  uint64_t end_frame = (base + len + PAGESIZE - 1) / PAGESIZE; /* round up */
  if (end_frame > PAGECOUNT)
    end_frame = PAGECOUNT;
  for (uint64_t f = start_frame; f < end_frame; f++) {
    frame_table[f].flags |= FRAME_RESERVED;
    if (!bit_test(f)) {
      bit_set(f);
      fc_dec();
    }
  }
}
// Round base UP and end DOWN so a partial frame at either edge is never handed
// out
static void mark_region_free(uint64_t base, uint64_t len) {
  if (len == 0)
    return;
  uint64_t start_frame = (base + PAGESIZE - 1) / PAGESIZE;
  uint64_t end_frame = (base + len) / PAGESIZE;
  if (end_frame > PAGECOUNT)
    end_frame = PAGECOUNT;
  for (uint64_t f = start_frame; f < end_frame; f++) {
    if (bit_test(f)) {
      bit_clear(f);
      frame_table[f].flags &= ~FRAME_RESERVED;
      fc_inc();
    }
  }
}
void pmm_init() {
  for (uint32_t i = 0; i < PAGECOUNT; i++) {
    bitmap[i] = 0xFF;
    frame_table[i] = (struct frame){.refcount = 0, .flags = FRAME_RESERVED};
  }
  free_count = 0;
  kprintf("[PMM] usable regions count: %d\n", count);

  // region 0 is skipped on purpose (low memory)
  for (int i = 1; i < count; i++)
    mark_region_free(__pinfo[i].base, __pinfo[i].len);

  // frame 0 stays reserved so PMM_NO_FRAME is unambiguous
  pmm_reserve_region(0, PAGESIZE);
  last_alloc_frame = 0;
  uint64_t k_phys_start = (uint64_t)(uintptr_t)kernel_start - KERNEL_BASE;
  uint64_t k_phys_end = (uint64_t)(uintptr_t)kernel_end - KERNEL_BASE;
  pmm_reserve_region(k_phys_start, k_phys_end - k_phys_start);
}

// Allocation / free
// Caller must hold pmm_lock.
uint64_t pmm_alloc_frame_locked(void) {
  if (free_count == 0)
    return PMM_NO_FRAME;
  // next-fit: scan from the hint, then wrap around once
  uint64_t pa = claim_free(last_alloc_frame, PAGECOUNT);
  if (pa == PMM_NO_FRAME)
    pa = claim_free(0, last_alloc_frame);
  if (pa == PMM_NO_FRAME) {
    kprintf("pmm: BUG free_count/bitmap mismatch\n");
    return PMM_NO_FRAME;
  }
  last_alloc_frame = (uint32_t)(pa / PAGESIZE) + 1;
  return pa;
}

uint64_t pmm_alloc_frame(void) {
  uint64_t fl = spin_lock_irqsave(&pmm_lock);
  uint64_t pa = pmm_alloc_frame_locked();
  spin_unlock_irqrestore(&pmm_lock, fl);
  return pa;
}

uint64_t pmm_alloc_frame_below(uint64_t limit) {
  if (free_count == 0)
    return PMM_NO_FRAME;
  uint64_t max_frame = limit / PAGESIZE;
  if (max_frame > PAGECOUNT)
    max_frame = PAGECOUNT;

  uint64_t fl = spin_lock_irqsave(&pmm_lock);
  uint64_t pa = claim_free(0, max_frame);
  spin_unlock_irqrestore(&pmm_lock, fl);

  if (pa == PMM_NO_FRAME)
    kprintf("pmm: BUG or OOM in pmm_alloc_frame_below, limit: %x\n", limit);
  return pa;
}

void pmm_free_frame(uint64_t addr) {
  if (addr % PAGESIZE != 0) {
    kprintf("pmm: BUG free_frame unaligned\n");
    return;
  }
  uint64_t frame = addr / PAGESIZE;
  // if (!pfn_valid(frame))
  // return;
  uint64_t fl = spin_lock_irqsave(&pmm_lock);
  int was_used = bit_test(frame) != 0;
  if (was_used) {
    bit_clear(frame);
    fc_inc();
  }
  spin_unlock_irqrestore(&pmm_lock, fl);

  if (!was_used)
    kprintf("pmm: BUG double free of frame %lx\n", addr);
}
// Stats and refcounting
uint64_t pmm_total_frames(void) { return PAGECOUNT; }
uint64_t pmm_free_frames(void) { return free_count; }
void frame_get(uint64_t pa) {
  uint64_t pfn = pa / PAGESIZE;
  if (!pfn_valid(pfn)) // not RAM (e.g. LAPIC MMIO)
    return;
  atomic_fetch_add_explicit(&frame_table[pfn].refcount, 1,
                            memory_order_relaxed);
}

void frame_put(uint64_t pa) {
  uint64_t pfn = pa / PAGESIZE;
  if (!pfn_valid(pfn))
    return;
  struct frame *f = &frame_table[pfn];
  if (f->flags & FRAME_RESERVED) /* kernel image, ACPI, ... */
    return;
  uint32_t old =
      atomic_fetch_sub_explicit(&f->refcount, 1, memory_order_acq_rel);
  if (old == 0)
    panic("[PMM] refcount underflow on frame %lx\n", pa);
  if (old == 1)
    pmm_free_frame(pa);
}

uint64_t frame_refs(uint64_t pa) {
  uint64_t pfn = pa / PAGESIZE;
  return pfn < PAGECOUNT ? frame_table[pfn].refcount : 0;
}

void print_frame_count(uint64_t f) {
  kprintf("[PMM] PA: %x, FRAME COUNT: %d\n", f, frame_refs(f));
}
