#include "../include/mm.h"
#include "../include/gpalloc.h"
#include <stdint.h>
#define ALIGN_UP4(size)  ((size)+(0x3) & ~(0x3))
static void *base_ptr;
void init_mem_alloc() {
 base_ptr = (void*)UHEAP_START;
}
void *alloc(uint64_t size) {
 int alignup_4bytes = ALIGN_UP4(size);
 void *mem = base_ptr;
 base_ptr = (char *)base_ptr + size;
 return mem;
}