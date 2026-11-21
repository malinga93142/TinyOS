#ifndef KBD_H
#define KBD_H

#include "../abi/kbd.h"

#define KBD_VECTOR 0x21
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

void kbd_init(void);
void kbd_irq(void);
int kbd_poll(struct key_event *ev);

#endif
