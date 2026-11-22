#ifndef MOUSE_H
#define MOUSE_H

#include "../abi/mouse.h"
#include <stdint.h>

#define MOUSE_VECTOR 0x2C

void mouse_init(uint32_t width, uint32_t height);
void mouse_irq(void);
void mouse_poll(void);
void mouse_selftest(void);
void mouse_get(struct mouse_state *out);

#endif
