/* kstring.h */
#ifndef KSTRING_H
#define KSTRING_H
#include <stddef.h>

void *kmemcpy(void *dst, const void *src, size_t n);
void *kmemset(void *dst, int val, size_t n);
int kmemcmp(const void *a, const void *b, size_t n);

#endif