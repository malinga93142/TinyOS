#ifndef FONT_H
#define FONT_H

#include <stdint.h>

extern const uint8_t font8x8_data[128][8];

static inline const uint8_t *font_glyph(char c) {
 unsigned char u = (unsigned char)c;
 return font8x8_data[u < 128 ? u : '?'];
}

#endif
