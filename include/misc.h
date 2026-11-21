#ifndef MISC_H
#define MISC_H
#define assert(expr)                                                   \
 do {                                                                  \
  if (!(expr)) {                                                       \
   panic("ASSERT FAILED (%s:%d): %s\n", __FILE__, __LINE__, #expr);    \
  }                                                                    \
 } while (0)
#include <stdint.h>
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t val);
uint16_t inw(uint16_t port);
void outl(uint16_t port, uint32_t val);
uint32_t inl(uint16_t port);
#endif  // MISC_H
