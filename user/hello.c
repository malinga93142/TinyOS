#include "syscall.h"
int main(void) {
 const char str[] = "Hello";
 sys_debug(0x1234);
 long i = sys_getpid(0x456);
 sys_write(0, (void *)str, sizeof(str)-1);
 return 0;
}
