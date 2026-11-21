#ifndef KMALLOC_H
#define KMALLOC_H
#include "mm.h"
#include "paging.h"
#include "spinlock.h"
#include "vmm.h"
#define SLAB_MIN_SIZE 32
#define SLAB_MAX_SIZE 1024
#define SLAB_HDR_SIZE 64
#define SLAB_NCACHES 6 // 32,64,128,256,512,1024

struct kcache;

struct slab {
  struct kcache *cache;
  void *free;               // first free object
  struct slab *next, *prev; // partial list (doubly linked)
  uint16_t inuse;
};

struct kcache {
  spinlock_t lock;
  const char *name;
  uint32_t objsize;   // requested size rounded up to align
  uint32_t align;     // power of two, <= 64
  uint32_t first_off; // offset of the first object in a slab page
  uint32_t objs_per_slab;
  struct slab *partial;      // slabs with at least one free object
  uint64_t nr_slabs;         // pages currently owned by this cache
  uint64_t nr_inuse;         // objects currently allocated
  struct kcache *next_cache; // registry link, for kcache_dump()
};

// Typed caches (size may be any value up to a page minus the header,
// align is a power of two <= 64, or 0 for 8).
void kcache_init(struct kcache *c, const char *name, uint64_t size,
                 uint64_t align);
struct kcache *kcache_create(const char *name, uint64_t size, uint64_t align);
void *kcache_alloc(struct kcache *c);
void *kcache_zalloc(struct kcache *c);
void kcache_free(void *obj);
void kcache_dump(void);

// General-purpose allocator
void kmalloc_init(void);
// defined vmm.h
// void *kmalloc(uint64_t bytes);
// void *kzalloc(uint64_t bytes);
// void kfree(void *ptr);
#endif // KMALLOC_H
