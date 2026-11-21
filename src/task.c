#include "../include/task.h"
#include "../include/cpu.h"
#include "../include/kmalloc.h"
#include "../include/misc.h"
#include "../include/list.h"
#include "../include/gdt.h"
#include "../include/percpu.h"
#include "../include/kstring.h"
#include "../include/mm.h"
#include "../include/paging.h"
#include "../include/printk.h"
#include "../include/spinlock.h"
#include <stdatomic.h>
#include <stdint.h>

#define SWITCH_SAVED_REGS 6

extern void switch_to(struct task *prev, struct task *next);

struct task *current;
DEFINE_PERCPU(struct task *, percpu_current);

static struct task boot_task;
static struct task idle_task;
static struct list_head task_list;
static spinlock_t task_lock;
static _Atomic uint32_t next_id = 1;

struct task *task_link(struct task *t) {
 if (!t)
  return NULL;
 uint64_t fl = spin_lock_irqsave(&task_lock);
 if (list_empty(&t->list))
  list_add_tail(&t->list, &task_list);
 spin_unlock_irqrestore(&task_lock, fl);
 return t;
}

struct task *task_unlink(struct task *t) {
 uint64_t fl = spin_lock_irqsave(&task_lock);
 int on_list = !list_empty(&t->list);
 if (on_list)
  list_del(&t->list);
 spin_unlock_irqrestore(&task_lock, fl);
 return on_list ? t : NULL;
}

struct task *task_find(uint32_t id) {
 struct task *t, *r = NULL;
 uint64_t fl = spin_lock_irqsave(&task_lock);
 list_for_each_entry(t, &task_list, list) {
  if (t->id == id) {
   r = t;
   break;
  }
 }
 spin_unlock_irqrestore(&task_lock, fl);
 return r;
}

static struct task *find_state(enum task_state s) {
 struct task *t, *r = NULL;
 uint64_t fl = spin_lock_irqsave(&task_lock);
 list_for_each_entry(t, &task_list, list) {
  if (t->state == s && t != &boot_task) {
   r = t;
   break;
  }
 }
 spin_unlock_irqrestore(&task_lock, fl);
 return r;
}

int task_list_is_empty(void) {
 uint64_t fl = spin_lock_irqsave(&task_lock);
 int empty = task_list.next == &boot_task.list &&
             boot_task.list.next == &task_list;
 spin_unlock_irqrestore(&task_lock, fl);
 return empty;
}

void task_dump(void) {
 struct task *t;
 uint64_t fl = spin_lock_irqsave(&task_lock);
 if (list_empty(&task_list))
  kprintf("[TASK] LIST IS EMPTY\n");
 list_for_each_entry(t, &task_list, list)
  kprintf("TASK[%p]: ID = %u, STATE = %d, CR3 = %lx, RSP = %lx\n", t, t->id,
          t->state, t->cr3, t->saved_rsp);
 spin_unlock_irqrestore(&task_lock, fl);
}

static int kstack_alloc(struct task *t) {
 uint64_t i;
 t->kstack_base = KSTACK_VA(t->id);
 for (i = 0; i < KSTACK_PAGES; i++) {
  uint64_t pa = pmm_alloc_frame();
  if (!pa || paging_map_page(t->kstack_base + i * PAGE_SIZE, pa,
                             PAGE_WRITE) == PAGING_ERR)
   break;
 }
 if (i < KSTACK_PAGES) {
  while (i--)
   paging_unmap_page(t->kstack_base + i * PAGE_SIZE);
  return -1;
 }
 t->kstack_top = t->kstack_base + KSTACK_PAGES * PAGE_SIZE;
 return 0;
}

static void kstack_free(struct task *t) {
 for (uint64_t i = 0; i < KSTACK_PAGES; i++)
  if (paging_unmap_page(t->kstack_base + i * PAGE_SIZE) == PAGING_ERR)
   panic("paging err");
}

static void kstack_prepare(struct task *t, uint64_t rip) {
 uint64_t *sp = (uint64_t *)t->kstack_top;
 *--sp = rip;
 for (int r = 0; r < SWITCH_SAVED_REGS; r++)
  *--sp = 0;
 t->saved_rsp = (uint64_t)sp;
}
extern void user_enter(void);
static void kstack_prepare_usr(struct task *t, uint64_t rip, uint64_t usp) {
 uint64_t *sp = (uint64_t *)t->kstack_top;
 *--sp = SEL_UDATA;
 *--sp = usp;
 *--sp = 0x202;
 *--sp = SEL_UCODE;
 *--sp = rip;
 *--sp = (uint64_t)user_enter;
 for (int r = 0; r < SWITCH_SAVED_REGS; r++)
  *--sp = 0;
 t->saved_rsp = (uint64_t)sp;
}
static struct task *pick_next(struct task *prev) {
 struct list_head *start = list_empty(&prev->list) ? &task_list : &prev->list;
 struct task *r = NULL;
 uint64_t fl = spin_lock_irqsave(&task_lock);
 for (struct list_head *n = start->next; n != start && !r; n = n->next) {
  struct task *t = list_entry(n, struct task, list);
  if (n != &task_list && t->state == TASK_READY)
   r = t;
 }
 if (!r && prev->state == TASK_DEAD)
  r = &idle_task;
 spin_unlock_irqrestore(&task_lock, fl);
 return r;
}

void task_schedule(void) {
 struct task *prev = current;
 struct task *next = pick_next(prev);
 if (!next || next == prev)
  return;
 if (prev->state == TASK_RUNNING)
  prev->state = TASK_READY;
 next->state = TASK_RUNNING;
 current = next;
 if (next->kstack_top)
  tss_set_rsp0(next->kstack_top);
 switch_to(prev, next);
}

static void idle_loop(void) {
 for (;;) {
  sti();
  hlt();
  asm("" ::: "memory");
 }
}

static void task_trampoline(void) {
 asm volatile("sti");
 struct task *self = current;
 if (self->entry)
  self->entry(self->arg);
 asm volatile("cli");
 self->state = TASK_DEAD;
 task_schedule();
 for (;;)
  asm volatile("hlt");
}

static int task_setup(struct task *t, uint64_t pml4) {
 list_init(&t->list);
 t->id = atomic_fetch_add(&next_id, 1);
 if (t->id >= KSTACK_MAX_TASKS || kstack_alloc(t) < 0)
  return -1;
 t->pml4_phys = pml4;
 t->cr3 = pml4 | t->pcid;
 t->state = TASK_READY;
 return 0;
}

void task_init(void) {
 list_init(&task_list);
 spinlock_init(&task_lock);
 list_init(&boot_task.list);
 boot_task.id = 0;
 boot_task.state = TASK_RUNNING;
 boot_task.cr3 = getcr3();
 boot_task.pml4_phys = boot_task.cr3 & PAGE_ADDR_MASK;
 task_link(&boot_task);
 current = &boot_task;
 int ret = task_setup(&idle_task, boot_task.pml4_phys);
 assert(ret == 0);
 kstack_prepare(&idle_task, (uint64_t)idle_loop);
}

struct task *task_create(void *(*entry)(void *), void *arg) {
 struct task *t = kzalloc(sizeof *t);
 if (!t)
  return NULL;
 t->mm = mm_create();
 if (!t->mm)
  goto fail;
 if (mm_mmap(t->mm, UHEAP_START, UHEAP_END, VMA_READ | VMA_WRITE, VMA_KIND_HEAP) < 0)
  goto fail_mm;
 if (mm_mmap(t->mm, USTACK_START,USTACK_END,VMA_READ | VMA_WRITE,VMA_KIND_STACK) < 0)
  goto fail_mm;
 if (task_setup(t, t->mm->pml4_phys) < 0)
  goto fail_mm;
 kstack_prepare(t, (uint64_t)task_trampoline);
 t->entry = entry;
 t->arg = arg;
 t->cpu_no = this_cpu();
 return task_link(t);
fail_mm:
 mm_destroy(t->mm);
fail:
 kfree(t);
 return NULL;
}

void task_destroy(struct task *t) {
 if (t == current)
  panic("[TASK] destroying running task %u\n", t->id);
 task_unlink(t);
 kstack_free(t);
 mm_destroy(t->mm);
 kfree(t);
}

void task_reap(void) {
 struct task *t;
 while ((t = find_state(TASK_DEAD)))
  task_destroy(t);
}

void task_reap_by_id(uint32_t id) {
 struct task *t = task_find(id);
 if (t)
  task_destroy(t);
}

void task_find_by_id(uint32_t id) {
 if (task_find(id))
  kprintf("found\n");
}

void task_run(struct task *t) {
 struct task *prev = current;
 t->state = TASK_RUNNING;
 current = t;
 switch_to(prev, t);
 kprintf("[TASK] task %u state=%d\n", t->id, t->state);
}

void task_run_all(void) {
 struct task *t;
 while ((t = find_state(TASK_READY))) {
  task_run(t);
  if (t->state == TASK_DEAD)
   task_destroy(t);
 }
}
void task_exit(void) {
 asm volatile("cli");
 current->state = TASK_DEAD;
 task_schedule();
 for (;;)
  asm volatile("hlt");
}

#define UTEXT  0x400000ULL
#define USTACK 0x7FFFFFFFB000ULL

struct task *task_create_user(const void *code, uint64_t len) {
 struct task *t = kzalloc(sizeof *t);
 if (!t) return NULL;
 t->mm = mm_create();
 if (!t->mm) goto fail;

 if (mm_mmap(t->mm, UTEXT, UTEXT + PAGE_SIZE, VMA_READ | VMA_EXEC, VMA_KIND_TEXT) < 0
  || mm_mmap(t->mm, USTACK, USTACK + PAGE_SIZE*4, VMA_READ|VMA_WRITE, VMA_KIND_STACK) < 0
  || task_setup(t, t->mm->pml4_phys) < 0)
  goto fail_mm;
 mm_copy_to(t->mm, UTEXT, code, len);
 kstack_prepare_usr(t, UTEXT, USTACK+PAGE_SIZE*4);
 t->cpu_no = this_cpu();
 return task_link(t);
fail_mm:
 mm_destroy(t->mm);
fail:
 kfree(t);
 return NULL;
}
