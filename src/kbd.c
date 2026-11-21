#include "../include/kbd.h"
#include "../include/idt.h"
#include "../include/lapic.h"
#include "../include/printk.h"
#include "../include/spinlock.h"
#include <stdint.h>

#define KBD_TRACE 0
#define KBD_BUF_SIZE 128
#define KBD_IRQ 1

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC_EOI 0x20

#define STATUS_OUT_FULL 0x01
#define STATUS_AUX 0x20

#define SC_RELEASE 0x80
#define SC_EXT_PREFIX 0xE0
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CTRL 0x1D
#define SC_ALT 0x38
#define SC_CAPS 0x3A

extern void isr33(void);

static const char keymap[2][0x3A] = {
 {0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\',
  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' '},
 {0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',
  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' '},
};

static struct key_event buf[KBD_BUF_SIZE];
static uint32_t head, tail;
static spinlock_t kbd_lock = SPINLOCK_INIT;
static uint8_t mods;
static int ext;

static inline void port_out8(uint16_t port, uint8_t v) {
 __asm__ volatile("outb %0, %1" ::"a"(v), "Nd"(port));
}

static inline uint8_t port_in8(uint16_t port) {
 uint8_t v;
 __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
 return v;
}

static void set_mod(uint8_t bit, int on) {
 if (on)
  mods |= bit;
 else
  mods &= ~bit;
}

static void push(const struct key_event *ev) {
 uint64_t fl = spin_lock_irqsave(&kbd_lock);
 if (head - tail < KBD_BUF_SIZE)
  buf[head++ % KBD_BUF_SIZE] = *ev;
 spin_unlock_irqrestore(&kbd_lock, fl);
}

static void handle_scancode(uint8_t sc) {
 if (sc == SC_EXT_PREFIX) {
  ext = 1;
  return;
 }
 int pressed = !(sc & SC_RELEASE);
 uint8_t code = sc & 0x7F;
 int was_ext = ext;
 ext = 0;

 if (!was_ext && (code == SC_LSHIFT || code == SC_RSHIFT)) {
  set_mod(KMOD_SHIFT, pressed);
  return;
 }
 if (code == SC_CTRL) {
  set_mod(KMOD_CTRL, pressed);
  return;
 }
 if (code == SC_ALT) {
  set_mod(KMOD_ALT, pressed);
  return;
 }
 if (!was_ext && code == SC_CAPS) {
  if (pressed)
   mods ^= KMOD_CAPS;
  return;
 }

 char c = 0;
 if (pressed && !was_ext && code < sizeof keymap[0]) {
  int shift = !!(mods & KMOD_SHIFT);
  char base = keymap[0][code];
  int letter = base >= 'a' && base <= 'z';
  if (letter)
   shift ^= !!(mods & KMOD_CAPS);
  c = keymap[shift][code];
  if (letter && (mods & KMOD_CTRL))
   c = base & 0x1F;
 }

 struct key_event ev = {
     .scancode = code | (was_ext ? KSC_EXT : 0),
     .pressed = pressed,
     .mods = mods,
     .ascii = c,
 };
#if KBD_TRACE
 kprintf("[KBD] sc=%x pressed=%d ascii=%d\n", ev.scancode, pressed, c);
#endif
 push(&ev);
}

void kbd_irq(void) {
 uint8_t status;
 while ((status = port_in8(KBD_STATUS_PORT)) & STATUS_OUT_FULL) {
  uint8_t sc = port_in8(KBD_DATA_PORT);
  if (!(status & STATUS_AUX))
   handle_scancode(sc);
 }
 port_out8(PIC1_CMD, PIC_EOI);
}

int kbd_poll(struct key_event *ev) {
 int got = 0;
 uint64_t fl = spin_lock_irqsave(&kbd_lock);
 if (head != tail) {
  *ev = buf[tail++ % KBD_BUF_SIZE];
  got = 1;
 }
 spin_unlock_irqrestore(&kbd_lock, fl);
 return got;
}

void kbd_init(void) {
 for (int i = 0; i < 16 && (port_in8(KBD_STATUS_PORT) & STATUS_OUT_FULL); i++)
  port_in8(KBD_DATA_PORT);
 add_idt(&IDT[KBD_VECTOR], 0, (uint64_t)(uintptr_t)isr33,
         IDT_INTERRUPT_GATE | IDT_PRESENT);
 port_out8(PIC1_DATA, port_in8(PIC1_DATA) & ~(1 << KBD_IRQ));
 lapic_enable_extint();
}
