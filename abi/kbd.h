#ifndef ABI_KBD_H
#define ABI_KBD_H

#include <stdint.h>

#define KMOD_SHIFT 0x01
#define KMOD_CTRL 0x02
#define KMOD_ALT 0x04
#define KMOD_CAPS 0x08

#define KSC_EXT 0x80

struct key_event {
 uint8_t scancode;
 uint8_t pressed;
 uint8_t mods;
 char ascii;
};

#endif
