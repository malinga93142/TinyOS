#ifndef PRINTK_H
#define PRINTK_H

#include <stdarg.h>

void kprintf(const char *fmt, ...);
void panic(const char *fmt, ...);
typedef void (*kputc_fn)(char c, void *ctx);
int kvformat(kputc_fn fn, void *ctx, const char *fmt, va_list ap);
int kvprintf(const char *fmt, va_list ap);
#endif
