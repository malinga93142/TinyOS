#ifndef SPINLOCK_H
#define SPINLOCK_H
#include <stdint.h>
#define RFLAGS_IF (1ULL << 9)
/* interrupt flag save/restore */
static inline uint64_t
irq_save(void)
{
  uint64_t flags;
  __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  return flags;
}
static inline void
irq_restore(uint64_t flags)
{
  if (flags & RFLAGS_IF) /* re-enable only if it was enabled before */
    __asm__ volatile("sti" : : : "memory");
}
/* test-and-test-and-set spinlock */
typedef struct
{
  volatile uint32_t v;
} spinlock_t;
#define SPINLOCK_INIT { 0 }
static inline void
spinlock_init(spinlock_t *lock)
{
  lock->v = 0;
}
static inline void
spin_lock(spinlock_t *l)
{
  while (__atomic_exchange_n(&l->v, 1, __ATOMIC_ACQUIRE))
    {
      while (__atomic_load_n(&l->v, __ATOMIC_RELAXED))
        __asm__ volatile("pause");
    }
}
static inline void
spin_unlock(spinlock_t *l)
{
  __atomic_store_n(&l->v, 0, __ATOMIC_RELEASE);
}
/* combined: disable IRQs, then lock */
static inline uint64_t
spin_lock_irqsave(spinlock_t *l)
{
  uint64_t flags = irq_save();
  spin_lock(l);
  return flags;
}
static inline void
spin_unlock_irqrestore(spinlock_t *l, uint64_t flags)
{
  spin_unlock(l);
  irq_restore(flags);
}
#endif
