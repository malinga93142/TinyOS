#include "../include/kmalloc.h"
#include "../include/kstring.h"
#include "../include/printk.h"
#include "../include/spinlock.h"
#include <stddef.h>

// Define KMALLOC_DEBUG to check every kcache_free for bad pointers and
// double frees (O(n) in the number of free objects in the slab).

#define KCACHE_MAX_ALIGN 64
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((uint64_t)(a) - 1))

_Static_assert(sizeof(struct slab) <= SLAB_HDR_SIZE, "slab header too big");

// Cache registry
// kcache_meta is a cache of struct kcache, so kcache_create() works
// without kmalloc. All caches are linked for kcache_dump().

static struct kcache kcache_meta;
static struct kcache caches[SLAB_NCACHES]; // kmalloc size classes
static char cache_names[SLAB_NCACHES][16];
static struct kcache *cache_list;
static spinlock_t cache_list_lock;

static void registry_add(struct kcache *c) {
  uint64_t fl = spin_lock_irqsave(&cache_list_lock);
  c->next_cache = cache_list;
  cache_list = c;
  spin_unlock_irqrestore(&cache_list_lock, fl);
}

void kcache_dump(void) {
  uint64_t fl = spin_lock_irqsave(&cache_list_lock);
  for (struct kcache *c = cache_list; c; c = c->next_cache)
    kprintf("[KCACHE] %s: objsize=%u slabs=%lu inuse=%lu\n", c->name,
            (unsigned)c->objsize, (unsigned long)c->nr_slabs,
            (unsigned long)c->nr_inuse);
  spin_unlock_irqrestore(&cache_list_lock, fl);
}

// Partial-slab list (caller holds c->lock)
// Only slabs with at least one free object are on it; full slabs are
// found again through the page-aligned header when an object is freed.

static void partial_push(struct kcache *c, struct slab *s) {
  s->prev = NULL;
  s->next = c->partial;
  if (c->partial)
    c->partial->prev = s;
  c->partial = s;
}

static void partial_unlink(struct kcache *c, struct slab *s) {
  if (s->prev)
    s->prev->next = s->next;
  else
    c->partial = s->next;
  if (s->next)
    s->next->prev = s->prev;
  s->next = s->prev = NULL;
}

// Slab pages (caller holds c->lock)
static struct slab *slab_new(struct kcache *c) {
  uint64_t pa = pmm_alloc_frame(); // owned by the slab
  if (!pa)
    return NULL;
  struct slab *s = (struct slab *)PA_TO_VA(pa);
  s->cache = c;
  s->inuse = 0;
  s->next = s->prev = NULL;
  s->free = NULL;
  uint8_t *base = (uint8_t *)s + c->first_off;
  for (uint64_t i = c->objs_per_slab; i-- > 0;) { // build back to front
    void *obj = base + i * c->objsize;
    *(void **)obj = s->free;
    s->free = obj;
  }
  c->nr_slabs++;
  return s;
}

#ifdef KMALLOC_DEBUG
static void check_free(struct kcache *c, struct slab *s, void *obj) {
  uint8_t *base = (uint8_t *)s + c->first_off;
  uint64_t off = (uint64_t)((uint8_t *)obj - base);
  if ((uint8_t *)obj < base || off / c->objsize >= c->objs_per_slab ||
      off % c->objsize)
    panic("[KCACHE] %s: bad free %lx\n", c->name, (unsigned long)obj);
  for (void *p = s->free; p; p = *(void **)p)
    if (p == obj)
      panic("[KCACHE] %s: double free %lx\n", c->name, (unsigned long)obj);
}
#else
#define check_free(c, s, obj) ((void)0)
#endif

// Cache API
//   size  : object size in bytes (any value, not only powers of two)
//   align : power of two <= KCACHE_MAX_ALIGN, or 0 for 8

void kcache_init(struct kcache *c, const char *name, uint64_t size,
                 uint64_t align) {
  if (!align)
    align = sizeof(void *);
  if (align & (align - 1) || align > KCACHE_MAX_ALIGN)
    panic("[KCACHE] %s: bad align %lu\n", name, (unsigned long)align);
  if (size < sizeof(void *)) // a free object holds the next pointer
    size = sizeof(void *);

  uint64_t objsize = ALIGN_UP(size, align);
  uint64_t first_off = ALIGN_UP(SLAB_HDR_SIZE, align);
  if (first_off + objsize > PAGE_SIZE)
    panic("[KCACHE] %s: objsize %lu too big\n", name, (unsigned long)size);

  c->name = name;
  c->objsize = (uint32_t)objsize;
  c->align = (uint32_t)align;
  c->first_off = (uint32_t)first_off;
  c->objs_per_slab = (uint32_t)((PAGE_SIZE - first_off) / objsize);
  c->partial = NULL;
  c->nr_slabs = 0;
  c->nr_inuse = 0;
  spinlock_init(&c->lock);
  registry_add(c);
}

// Create a cache for a fixed-size type, e.g.
// kcache_create("task", sizeof(struct task), 64);
// Needs kmalloc_init() to have run. Returns NULL on out-of-memory.
struct kcache *kcache_create(const char *name, uint64_t size, uint64_t align) {
  struct kcache *c = kcache_zalloc(&kcache_meta);
  if (!c)
    return NULL;
  kcache_init(c, name, size, align);
  return c;
}

void *kcache_alloc(struct kcache *c) {
  uint64_t fl = spin_lock_irqsave(&c->lock);

  struct slab *s = c->partial;
  if (!s) {
    s = slab_new(c);
    if (!s) {
      spin_unlock_irqrestore(&c->lock, fl);
      return NULL;
    }
    partial_push(c, s);
  }

  void *obj = s->free;
  s->free = *(void **)obj;
  s->inuse++;
  c->nr_inuse++;
  if (!s->free) // full: take it off the partial list
    partial_unlink(c, s);

  spin_unlock_irqrestore(&c->lock, fl);
  return obj;
}

void *kcache_zalloc(struct kcache *c) {
  void *obj = kcache_alloc(c);
  if (obj)
    kmemset(obj, 0, c->objsize);
  return obj;
}

void kcache_free(void *obj) {
  if (!obj)
    return;

  struct slab *s = (struct slab *)((uint64_t)obj & ~PAGE_OFFSET_MASK_4K);
  struct kcache *c = s->cache;
  struct slab *release = NULL;

  uint64_t fl = spin_lock_irqsave(&c->lock);
  check_free(c, s, obj);

  int was_full = (s->free == NULL);
  *(void **)obj = s->free;
  s->free = obj;
  s->inuse--;
  c->nr_inuse--;
  if (was_full)
    partial_push(c, s);

  if (s->inuse == 0) { // empty: return the page, but keep one spare
    partial_unlink(c, s);
    if (c->partial) {
      c->nr_slabs--;
      release = s;
    } else {
      partial_push(c, s);
    }
  }
  spin_unlock_irqrestore(&c->lock, fl);

  if (release)
    pmm_free_frame(VA_TO_PA(release));
}
// General-purpose allocator (power-of-two size classes)
static void name_class(char *buf, uint64_t size) {
  const char *prefix = "kmalloc-";
  char digits[20];
  int n = 0, p = 0;
  while (*prefix)
    buf[p++] = *prefix++;
  do {
    digits[n++] = (char)('0' + size % 10);
    size /= 10;
  } while (size);
  while (n)
    buf[p++] = digits[--n];
  buf[p] = '\0';
}
void kmalloc_init(void) {
  spinlock_init(&cache_list_lock);
  kcache_init(&kcache_meta, "kcache", sizeof(struct kcache), 0);

  for (int i = 0; i < SLAB_NCACHES; i++) {
    uint64_t size = (uint64_t)SLAB_MIN_SIZE << i;
    name_class(cache_names[i], size);
    kcache_init(&caches[i], cache_names[i], size, 0);
  }
}
void *kmalloc(uint64_t bytes) {
  if (!bytes)
    return NULL;
  if (bytes > SLAB_MAX_SIZE)
    return vmalloc(PAGES_FOR(bytes));
  int i = 0;
  uint64_t sz = SLAB_MIN_SIZE;
  while (sz < bytes) {
    sz <<= 1;
    i++;
  }
  return kcache_alloc(&caches[i]);
}
void *kzalloc(uint64_t bytes) {
  void *p = kmalloc(bytes);
  if (p)
    kmemset(p, 0, bytes);
  return p;
}
// Works for kmalloc memory and for objects from any kcache.
void kfree(void *ptr) {
  if (!ptr)
    return;
  if (IS_VMALLOC_ADDR((uint64_t)ptr))
    vfree(ptr);
  else
    kcache_free(ptr);
}
