#include "../include/task.h"
#include "../include/printk.h"
static void *some(void *a) {
 (void)a;
 asm volatile(
  "movq $0x494e415641524853, %%rax\n\t"
  "push %%rax\n\t"
  "pop %%rax"
  :::"rax"
 );
 kprintf("Hello\n");
 return NULL;
}
void test() {
 struct task *t = task_create(some, 0);
 task_run(t);
}
