#include "../include/gfx.h"
#include "../include/font.h"
#include "../include/kstring.h"
#include "../include/spinlock.h"

#define FB_SCALE 1
#define CELL (8 * FB_SCALE)
#define TAB_WIDTH 8

static struct framebuffer *con;
static spinlock_t con_lock = SPINLOCK_INIT;
static uint32_t cols, rows, cur_x, cur_y;
static uint32_t cur_fg = COLOR_RED, cur_bg = COLOR_BLACK;

static inline uint32_t *row_ptr(struct framebuffer *fb, uint32_t y) {
 return (uint32_t *)((uint8_t *)(uintptr_t)fb->addr + (uint64_t)y * fb->pitch);
}

void gfx_putpixel(struct framebuffer *fb, uint32_t x, uint32_t y,
                  uint32_t color) {
 if (x >= fb->width || y >= fb->height)
  return;
 row_ptr(fb, y)[x] = color;
}

void gfx_draw_char(struct framebuffer *fb, uint32_t x, uint32_t y, char c,
                   uint32_t color, uint32_t scale) {
 const uint8_t *glyph = font_glyph(c);
 for (int row = 0; row < 8; row++) {
  for (int col = 0; col < 8; col++) {
   if (!(glyph[row] & (0x80 >> col)))
    continue;
   for (uint32_t sy = 0; sy < scale; sy++)
    for (uint32_t sx = 0; sx < scale; sx++)
     gfx_putpixel(fb, x + col * scale + sx, y + row * scale + sy, color);
  }
 }
}

void gfx_draw_string(struct framebuffer *fb, uint32_t x, uint32_t y,
                     const char *s, uint32_t color, uint32_t scale) {
 for (; *s; s++, x += 8 * scale)
  gfx_draw_char(fb, x, y, *s, color, scale);
}

static void draw_cell(char c, uint32_t col, uint32_t row) {
 const uint8_t *glyph = font_glyph(c);
 for (uint32_t y = 0; y < CELL; y++) {
  uint32_t *p = row_ptr(con, row * CELL + y) + col * CELL;
  uint8_t bits = glyph[y / FB_SCALE];
  for (uint32_t x = 0; x < CELL; x++)
   p[x] = (bits & (0x80 >> (x / FB_SCALE))) ? cur_fg : cur_bg;
 }
}

static void fill_rows(uint32_t y0, uint32_t y1, uint32_t color) {
 for (uint32_t y = y0; y < y1; y++) {
  uint32_t *p = row_ptr(con, y);
  for (uint32_t x = 0; x < con->width; x++)
   p[x] = color;
 }
}

static void scroll_line(void) {
 uint32_t h = rows * CELL;
 for (uint32_t y = 0; y + CELL < h; y++)
  kmemcpy(row_ptr(con, y), row_ptr(con, y + CELL), con->width * 4);
 fill_rows(h - CELL, h, cur_bg);
}

static void newline(void) {
 cur_x = 0;
 if (++cur_y >= rows) {
  scroll_line();
  cur_y = rows - 1;
 }
}

static void put(char c) {
 switch (c) {
 case '\n':
  newline();
  return;
 case '\r':
  cur_x = 0;
  return;
 case '\t':
  cur_x = (cur_x + TAB_WIDTH) & ~(uint32_t)(TAB_WIDTH - 1);
  if (cur_x >= cols)
   newline();
  return;
 case '\b':
  if (cur_x)
   draw_cell(' ', --cur_x, cur_y);
  return;
 }
 if (cur_x >= cols)
  newline();
 draw_cell(c, cur_x++, cur_y);
}

void fb_init(struct framebuffer *fb) {
 // uint64_t fl = spin_lock_irqsave(&con_lock);
 con = fb;
 cols = fb->width / CELL;
 rows = fb->height / CELL;
 cur_x = cur_y = 0;
 fill_rows(0, fb->height, cur_bg);
 // spin_unlock_irqrestore(&con_lock, fl);
}

size_t write_fb(const char *buf, size_t len) {
 uint64_t fl = spin_lock_irqsave(&con_lock);
 if (con)
  for (size_t i = 0; i < len; i++)
   put(buf[i]);
 spin_unlock_irqrestore(&con_lock, fl);
 return len;
}

void putc_fb(char c) { write_fb(&c, 1); }

void puts_fb(const char *s) {
 size_t n = 0;
 while (s[n])
  n++;
 write_fb(s, n);
}

void setcolor_fb(uint32_t fg, uint32_t bg) {
 uint64_t fl = spin_lock_irqsave(&con_lock);
 cur_fg = fg;
 cur_bg = bg;
 spin_unlock_irqrestore(&con_lock, fl);
}

void clear_fb(void) {
 uint64_t fl = spin_lock_irqsave(&con_lock);
 if (con) {
  cur_x = cur_y = 0;
  fill_rows(0, con->height, cur_bg);
 }
 spin_unlock_irqrestore(&con_lock, fl);
}
