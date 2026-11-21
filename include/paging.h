// 63             52 51        12 11        9 8 7 6 5 4   3   2 1 0
// +----------------+------------+-----------+-+--+-+-+---+---+-+---+
// | NX             | Phys Addr  | Software  |G|PS|D|A|PCD|PWT|U|W|P|
// +----------------+------------+-----------+-+--+-+-+---+---+-+---+
//
#ifndef PAGING_H
#define PAGING_H
#include "mm.h"
#include "pmm.h"
#include <stdint.h>
#define PAGING_ERR (~0ULL)
typedef uint64_t pte_t;
void paging_init(uint64_t kernel_phys_start, uint64_t kernel_phys_end);
void page_fault_map(uint64_t addr);
uint64_t paging_map_page_in(uint64_t root_phys, uint64_t va, uint64_t pa,
                            uint64_t flags, int flush);
uint64_t paging_unmap_page_in(uint64_t root_phys, uint64_t va, int flush);
uint64_t paging_map_page(uint64_t va, uint64_t pa, uint64_t flags);
uint64_t paging_map_frame(uint64_t va, uint64_t pa, uint64_t flags);
uint64_t paging_unmap_page(uint64_t va);
uint64_t paging_unmap_frame(uint64_t va);
void unmap_lower_half(void);
uint64_t paging_virt_to_phys(uint64_t);
int aspace_map(uint64_t, uint64_t, uint64_t, uint64_t);
pte_t *paging_walk(uint64_t pml4_phys, uint64_t va, int *level);
void pml4_free(uint64_t);
#endif
