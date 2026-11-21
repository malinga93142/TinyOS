#include "../include/hpet.h"
static hpet_dev_t hpet;
static inline uint64_t hpet_read(uint32_t off) {
  return *(volatile uint64_t *)(hpet.base_virt + off);
}
static inline void hpet_write(uint32_t off, uint64_t v) {
  *(volatile uint64_t *)(hpet.base_virt + off) = v;
}
void setup_hpet(uint64_t base) {
  hpet.base_virt = (volatile uint8_t *)base;
  hpet.ready = 0;
  uint64_t caps = hpet_read(HPET_GENERAL_CAPS);
  hpet.period_fs = (uint32_t)(caps >> HPET_CAP_PERIOD_SHIFT);
  if (hpet.period_fs == 0 || hpet.period_fs > HPET_MAX_PERIOD_FS)
    hpet.period_fs = HPET_FALLBACK_PERIOD_FS;
  // a 32-bit counter wraps at 2^32, so deltas must be masked
  hpet.counter_mask = (caps & HPET_CAP_COUNT_SIZE) ? ~0ULL : 0xFFFFFFFFULL;
  hpet_write(HPET_GENERAL_CONF, hpet_read(HPET_GENERAL_CONF) | HPET_CONF_ENABLE);
  // bounded check that the counter actually runs, so later waits can't hang
  uint64_t start = hpet_get_counter();
  for (uint32_t i = 0; i < 1000000; i++) {
    if (hpet_get_counter() != start) {
      hpet.ready = 1;
      break;
    }
  }
}
int hpet_is_ready(void) { return hpet.ready; }
uint32_t hpet_period_fs(void) { return hpet.period_fs; }
uint64_t hpet_get_counter(void) { return hpet_read(HPET_MAIN_COUNTER); }
uint64_t hpet_us_to_ticks(uint64_t us) {
  return (us * HPET_FS_PER_US) / hpet.period_fs;
}
// busy-wait, returns immediately if the HPET never started
void hpet_delay_us(uint64_t us) {
  if (!hpet.ready)
    return;
  uint64_t ticks = hpet_us_to_ticks(us);
  uint64_t start = hpet_get_counter();
  while (((hpet_get_counter() - start) & hpet.counter_mask) < ticks)
    __asm__ volatile("pause");
}
