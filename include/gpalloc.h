#ifndef GPALLOC_H
#define GPALLOC_H
#include <stdint.h>
void init_mem_alloc();
void *alloc(uint64_t size);
#endif