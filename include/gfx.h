#ifndef GFX_H
#define GFX_H

#include "mb.h"
#include <stddef.h>
#include <stdint.h>

#define RGB(r, g, b)                                                           \
 ((((uint32_t)(r) & 0xFF) << 16) | (((uint32_t)(g) & 0xFF) << 8) |           \
  ((uint32_t)(b) & 0xFF))
#define COLOR_BLACK RGB(0x00, 0x00, 0x00)
#define COLOR_WHITE RGB(0xFF, 0xFF, 0xFF)
#define COLOR_RED RGB(0xFF, 0x00, 0x00)
#define COLOR_GREEN RGB(0x00, 0xFF, 0x00)
#define COLOR_BLUE RGB(0x00, 0x00, 0xFF)
#define COLOR_YELLOW RGB(0xFF, 0xFF, 0x00)
#define COLOR_GRAY RGB(0x80, 0x80, 0x80)

void fb_init(struct framebuffer *fb);
size_t write_fb(const char *buf, size_t len);
void putc_fb(char c);
void puts_fb(const char *s);
void setcolor_fb(uint32_t fg, uint32_t bg);
void clear_fb(void);

void gfx_putpixel(struct framebuffer *fb, uint32_t x, uint32_t y,
                  uint32_t color);
void gfx_draw_char(struct framebuffer *fb, uint32_t x, uint32_t y, char c,
                   uint32_t color, uint32_t scale);
void gfx_draw_string(struct framebuffer *fb, uint32_t x, uint32_t y,
                     const char *s, uint32_t color, uint32_t scale);

#endif