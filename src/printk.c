#include "../include/printk.h"
#include "../include/spinlock.h"
#include "../include/uart.h"
#include "../include/gfx.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define F_LEFT  0x01
#define F_PLUS  0x02
#define F_SPACE 0x04
#define F_ALT   0x08
#define F_ZERO  0x10

enum { LEN_HH, LEN_H, LEN_NONE, LEN_L, LEN_LL, LEN_Z, LEN_J, LEN_T };

struct out {
 kputc_fn fn;
 void *ctx;
 int n;
};

static void emit(struct out *o, char c) {
 o->fn(c, o->ctx);
 o->n++;
}

static void emit_rep(struct out *o, char c, int n) {
 while (n-- > 0)
  emit(o, c);
}

static void fmt_str(struct out *o, const char *s, int len, int flags,
                    int width) {
 int pad = width - len;
 if (!(flags & F_LEFT))
  emit_rep(o, ' ', pad);
 for (int i = 0; i < len; i++)
  emit(o, s[i]);
 if (flags & F_LEFT)
  emit_rep(o, ' ', pad);
}

static void fmt_number(struct out *o, uint64_t v, int neg, int is_signed,
                       int base, int upper, int flags, int width, int prec) {
 const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
 char buf[24];
 int nd = 0;
 char sign = 0;
 char prefix[2];
 int plen = 0;
 int vzero = (v == 0);

 if (is_signed) {
  if (neg)
   sign = '-';
  else if (flags & F_PLUS)
   sign = '+';
  else if (flags & F_SPACE)
   sign = ' ';
 }

 if (!(vzero && prec == 0)) {
  do {
   buf[nd++] = digits[v % base];
   v /= base;
  } while (v);
 }

 if (base == 16 && (flags & F_ALT) && !vzero) {
  prefix[0] = '0';
  prefix[1] = upper ? 'X' : 'x';
  plen = 2;
 }

 int zeros = prec > nd ? prec - nd : 0;
 if (base == 8 && (flags & F_ALT) && zeros == 0 &&
     (nd == 0 || buf[nd - 1] != '0'))
  zeros = 1;

 if (prec >= 0 || (flags & F_LEFT))
  flags &= ~F_ZERO;

 int total = (sign ? 1 : 0) + plen + zeros + nd;
 int pad = width - total;

 if (!(flags & (F_LEFT | F_ZERO)))
  emit_rep(o, ' ', pad);
 if (sign)
  emit(o, sign);
 for (int i = 0; i < plen; i++)
  emit(o, prefix[i]);
 if (flags & F_ZERO)
  emit_rep(o, '0', pad);
 emit_rep(o, '0', zeros);
 while (nd--)
  emit(o, buf[nd]);
 if (flags & F_LEFT)
  emit_rep(o, ' ', pad);
}

int kvformat(kputc_fn fn, void *ctx, const char *fmt, va_list ap) {
 struct out o = {fn, ctx, 0};

 for (; *fmt; fmt++) {
  if (*fmt != '%') {
   emit(&o, *fmt);
   continue;
  }
  fmt++;

  int flags = 0;
  for (;; fmt++) {
   if (*fmt == '-')
    flags |= F_LEFT;
   else if (*fmt == '+')
    flags |= F_PLUS;
   else if (*fmt == ' ')
    flags |= F_SPACE;
   else if (*fmt == '#')
    flags |= F_ALT;
   else if (*fmt == '0')
    flags |= F_ZERO;
   else
    break;
  }

  int width = 0;
  if (*fmt == '*') {
   width = va_arg(ap, int);
   if (width < 0) {
    flags |= F_LEFT;
    width = -width;
   }
   fmt++;
  } else {
   while (*fmt >= '0' && *fmt <= '9')
    width = width * 10 + (*fmt++ - '0');
  }

  int prec = -1;
  if (*fmt == '.') {
   fmt++;
   prec = 0;
   if (*fmt == '*') {
    prec = va_arg(ap, int);
    fmt++;
   } else {
    while (*fmt >= '0' && *fmt <= '9')
     prec = prec * 10 + (*fmt++ - '0');
   }
  }

  int len = LEN_NONE;
  switch (*fmt) {
  case 'h':
   fmt++;
   if (*fmt == 'h') {
    len = LEN_HH;
    fmt++;
   } else {
    len = LEN_H;
   }
   break;
  case 'l':
   fmt++;
   if (*fmt == 'l') {
    len = LEN_LL;
    fmt++;
   } else {
    len = LEN_L;
   }
   break;
  case 'z':
   len = LEN_Z;
   fmt++;
   break;
  case 'j':
   len = LEN_J;
   fmt++;
   break;
  case 't':
   len = LEN_T;
   fmt++;
   break;
  }

  switch (*fmt) {
  case '%':
   emit(&o, '%');
   break;

  case 'c': {
   char c = (char)va_arg(ap, int);
   fmt_str(&o, &c, 1, flags, width);
   break;
  }

  case 's': {
   const char *s = va_arg(ap, const char *);
   if (!s)
    s = "(null)";
   int n = 0;
   while (s[n] && (prec < 0 || n < prec))
    n++;
   fmt_str(&o, s, n, flags, width);
   break;
  }

  case 'p': {
   uintptr_t p = (uintptr_t)va_arg(ap, void *);
   if (!p)
    fmt_str(&o, "(nil)", 5, flags, width);
   else
    fmt_number(&o, p, 0, 0, 16, 0, flags | F_ALT, width, prec);
   break;
  }

  case 'd':
  case 'i': {
   int64_t sv;
   switch (len) {
   case LEN_HH:
    sv = (signed char)va_arg(ap, int);
    break;
   case LEN_H:
    sv = (short)va_arg(ap, int);
    break;
   case LEN_L:
    sv = va_arg(ap, long);
    break;
   case LEN_LL:
    sv = va_arg(ap, long long);
    break;
   case LEN_Z:
    sv = va_arg(ap, intptr_t);
    break;
   case LEN_J:
    sv = va_arg(ap, intmax_t);
    break;
   case LEN_T:
    sv = va_arg(ap, ptrdiff_t);
    break;
   default:
    sv = va_arg(ap, int);
    break;
   }
   int neg = sv < 0;
   uint64_t mag = neg ? (uint64_t)0 - (uint64_t)sv : (uint64_t)sv;
   fmt_number(&o, mag, neg, 1, 10, 0, flags, width, prec);
   break;
  }

  case 'u':
  case 'o':
  case 'x':
  case 'X': {
   uint64_t uv;
   switch (len) {
   case LEN_HH:
    uv = (unsigned char)va_arg(ap, unsigned int);
    break;
   case LEN_H:
    uv = (unsigned short)va_arg(ap, unsigned int);
    break;
   case LEN_L:
    uv = va_arg(ap, unsigned long);
    break;
   case LEN_LL:
    uv = va_arg(ap, unsigned long long);
    break;
   case LEN_Z:
    uv = va_arg(ap, size_t);
    break;
   case LEN_J:
    uv = va_arg(ap, uintmax_t);
    break;
   case LEN_T:
    uv = (uintptr_t)va_arg(ap, ptrdiff_t);
    break;
   default:
    uv = va_arg(ap, unsigned int);
    break;
   }
   int base = *fmt == 'u' ? 10 : *fmt == 'o' ? 8 : 16;
   fmt_number(&o, uv, 0, 0, base, *fmt == 'X', flags, width, prec);
   break;
  }

  case '\0':
   fmt--;
   break;

  default:
   emit(&o, '%');
   emit(&o, *fmt);
   break;
  }
 }
 return o.n;
}
extern int activate_from_fb_from_here;
static void uart_sink(char c, void *ctx) {
 (void)ctx;
 uart_putc(c);
 if (activate_from_fb_from_here == 1) {}
  // putc_fb(c);
}

static void puts(const char *s) {
 while (*s)
  uart_putc(*s++);
}

static spinlock_t kprintf_lock = SPINLOCK_INIT;

int kvprintf(const char *fmt, va_list ap) {
 uint64_t f = spin_lock_irqsave(&kprintf_lock);
 int n = kvformat(uart_sink, 0, fmt, ap);
 spin_unlock_irqrestore(&kprintf_lock, f);
 return n;
}

void kprintf(const char *fmt, ...) {
 va_list ap;
 va_start(ap, fmt);
 kvprintf(fmt, ap);
 va_end(ap);
}

void panic(const char *fmt, ...) {
 va_list ap;
 puts("\n\n[KERNEL PANIC]\n");
 va_start(ap, fmt);
 kvformat(uart_sink, 0, fmt, ap);
 va_end(ap);
 puts("\n");
 for (;;)
  asm volatile("cli; hlt");
}
