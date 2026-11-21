#ifndef HPET_H
#define HPET_H
#include <stdint.h>

// Register offsets (bytes)
#define HPET_GENERAL_CAPS 0x000
#define HPET_GENERAL_CONF 0x010
#define HPET_MAIN_COUNTER 0x0F0

// General capabilities bits
#define HPET_CAP_COUNT_SIZE (1ULL << 13) // 1 = 64-bit main counter
#define HPET_CAP_PERIOD_SHIFT 32         // counter period in fs, bits 63:32

// General configuration bits
#define HPET_CONF_ENABLE (1ULL << 0)

// Spec: period must be nonzero and at most 100 ns
#define HPET_MAX_PERIOD_FS 100000000UL
#define HPET_FALLBACK_PERIOD_FS 69841272UL // ~14.318 MHz
#define HPET_FS_PER_US 1000000000ULL

typedef struct {
  volatile uint8_t *base_virt;
  uint32_t period_fs; // femtoseconds per tick
  uint64_t counter_mask;
  int ready;
} hpet_dev_t;

void setup_hpet(uint64_t base);
int hpet_is_ready(void);
uint32_t hpet_period_fs(void);
uint64_t hpet_get_counter(void);
uint64_t hpet_us_to_ticks(uint64_t us);
void hpet_delay_us(uint64_t us);

#endif // HPET_H
