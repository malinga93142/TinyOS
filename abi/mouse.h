#ifndef ABI_MOUSE_H
#define ABI_MOUSE_H

#include <stdint.h>

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04

struct mouse_state {
 int32_t x;
 int32_t y;
 int32_t wheel;
 uint32_t buttons;
 uint32_t seq;
};

#endif
