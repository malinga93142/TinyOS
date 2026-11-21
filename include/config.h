#ifndef CONFIG_H
#define CONFIG_H

/* 1 = program LVT LINT1 as NMI on every CPU during LAPIC init.
 * 0 = leave LINT1 alone (BSP keeps firmware state, AP stays masked). */
#ifndef CONFIG_LAPIC_NMI
#define CONFIG_LAPIC_NMI 0
#endif

#endif
