#include "../include/mouse.h"
#include "../include/idt.h"
#include "../include/misc.h"
#include "../include/printk.h"
#include "../include/spinlock.h"
#include <stdint.h>

#define MOUSE_TRACE 1
#define WAIT_LOOPS 100000

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64
#define ST_OUT_FULL 0x01
#define ST_IN_FULL 0x02
#define ST_AUX 0x20

#define CTL_READ_CFG 0x20
#define CTL_WRITE_CFG 0x60
#define CTL_ENABLE_AUX 0xA8
#define CTL_TO_MOUSE 0xD4
#define CFG_IRQ12 0x02
#define CFG_AUX_CLOCK_OFF 0x20

#define MSE_ACK 0xFA
#define MSE_GET_ID 0xF2
#define MSE_SET_RATE 0xF3
#define MSE_ENABLE 0xF4
#define MSE_DEFAULTS 0xF6

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
#define PIC1_CASCADE_BIT 2
#define PIC2_MOUSE_BIT 4

extern void isr44(void);

static struct mouse_state state;
static spinlock_t mouse_lock = SPINLOCK_INIT;
static int32_t max_x, max_y;
static uint8_t pkt[4];
static int pkt_idx, pkt_len = 3;

static int wait_write(void) {
 for (int i = 0; i < WAIT_LOOPS; i++)
  if (!(inb(PS2_STATUS) & ST_IN_FULL))
   return 0;
 return -1;
}

static int read_data(void) {
 for (int i = 0; i < WAIT_LOOPS; i++)
  if (inb(PS2_STATUS) & ST_OUT_FULL)
   return inb(PS2_DATA);
 return -1;
}

static int ctl_cmd(uint8_t c) {
 if (wait_write())
  return -1;
 outb(PS2_CMD, c);
 return 0;
}

static int ctl_data(uint8_t d) {
 if (wait_write())
  return -1;
 outb(PS2_DATA, d);
 return 0;
}

static int mouse_cmd(uint8_t b) {
 if (ctl_cmd(CTL_TO_MOUSE) || ctl_data(b))
  return -1;
 return read_data() == MSE_ACK ? 0 : -1;
}

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) {
 return v < lo ? lo : v > hi ? hi : v;
}

static void handle_byte(uint8_t b) {
 if (pkt_idx == 0 && !(b & 0x08))
  return;
 pkt[pkt_idx++] = b;
 if (pkt_idx < pkt_len)
  return;
 pkt_idx = 0;

 uint8_t f = pkt[0];
 if (f & 0xC0)
  return;
 int32_t dx = pkt[1] - ((f << 4) & 0x100);
 int32_t dy = pkt[2] - ((f << 3) & 0x100);
 int32_t dz = 0;
 if (pkt_len == 4) {
  dz = pkt[3] & 0x0F;
  if (dz & 8)
   dz -= 16;
 }

 uint64_t fl = spin_lock_irqsave(&mouse_lock);
 state.x = clamp(state.x + dx, 0, max_x);
 state.y = clamp(state.y - dy, 0, max_y);
 state.buttons = f & 7;
 state.wheel += dz;
 state.seq++;
 spin_unlock_irqrestore(&mouse_lock, fl);
#if MOUSE_TRACE
 kprintf("[MOUSE] x=%d y=%d btn=%x wheel=%d\n", state.x, state.y,
         state.buttons, state.wheel);
#endif
}

void mouse_poll(void) {
 while ((inb(PS2_STATUS) & (ST_OUT_FULL | ST_AUX)) ==
        (ST_OUT_FULL | ST_AUX))
  handle_byte(inb(PS2_DATA));
}

void mouse_irq(void) {
 mouse_poll();
 outb(PIC2_CMD, PIC_EOI);
 outb(PIC1_CMD, PIC_EOI);
}

void mouse_get(struct mouse_state *out) {
 uint64_t fl = spin_lock_irqsave(&mouse_lock);
 *out = state;
 spin_unlock_irqrestore(&mouse_lock, fl);
}

void mouse_selftest(void) {
 if (mouse_cmd(0xEB)) {
  kprintf("[MOUSE] selftest: no ACK\n");
  return;
 }
 for (int i = 0; i < pkt_len; i++)
  kprintf("[MOUSE] selftest byte %d = %x\n", i, read_data());
}

void mouse_init(uint32_t width, uint32_t height) {
 max_x = width - 1;
 max_y = height - 1;
 state.x = width / 2;
 state.y = height / 2;

 for (int i = 0; i < 16 && (inb(PS2_STATUS) & ST_OUT_FULL); i++)
  inb(PS2_DATA);

 if (ctl_cmd(CTL_ENABLE_AUX) || ctl_cmd(CTL_READ_CFG))
  goto fail;
 int cfg = read_data();
 if (cfg < 0)
  goto fail;
 cfg = (cfg | CFG_IRQ12) & ~CFG_AUX_CLOCK_OFF;
 if (ctl_cmd(CTL_WRITE_CFG) || ctl_data((uint8_t)cfg))
  goto fail;

 if (mouse_cmd(MSE_DEFAULTS))
  goto fail;
 mouse_cmd(MSE_SET_RATE), mouse_cmd(200);
 mouse_cmd(MSE_SET_RATE), mouse_cmd(100);
 mouse_cmd(MSE_SET_RATE), mouse_cmd(80);
 if (mouse_cmd(MSE_GET_ID))
  goto fail;
 int id = read_data();
 pkt_len = (id == 3 || id == 4) ? 4 : 3;
 if (mouse_cmd(MSE_ENABLE))
  goto fail;

 add_idt(&IDT[MOUSE_VECTOR], 0, (uint64_t)(uintptr_t)isr44,
         IDT_INTERRUPT_GATE | IDT_PRESENT);
 outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << PIC1_CASCADE_BIT));
 outb(PIC2_DATA, inb(PIC2_DATA) & ~(1 << PIC2_MOUSE_BIT));
 kprintf("[MOUSE] INIT id=%d packet=%d\n", id, pkt_len);
 return;
fail:
 kprintf("[MOUSE] not present\n");
}
