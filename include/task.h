#ifndef TASK_H
#define TASK_H
#include "percpu.h"
#include "list.h"
#include <stdint.h>
// @author Malinga RK.
// task.h
#define TASK_TEXT_BASE 0x0000000000400000ULL // private code/data
#define TASK_HEAP_BASE 0x0000000010000000ULL // private heap, grows up
#define TASK_HEAP_END 0x00007F0000000000ULL

#define KSTACK_PAGES 4
#define KSTACK_SLOT ((KSTACK_PAGES + 1) * PAGE_SIZE)
#define KSTACK_VA(id) (KSTACK_BASE + (uint64_t)(id) * KSTACK_SLOT)
#define KSTACK_MAX_TASKS ((KSTACK_END - KSTACK_BASE) / KSTACK_SLOT)

enum task_state { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_DEAD };

struct ctx_regs {
	uint64_t r15, r14, r13, r12, rbx, rbp;
	uint64_t rip;
};
struct task {
  uint64_t saved_rsp; // asm can use offset 0
  uint64_t cr3;       // PML4 phys | pcid
  uint64_t pml4_phys;
  uint16_t pcid;
  uint32_t id;
  enum task_state state;
  uint64_t kstack_base;
  uint64_t kstack_top; // initial rsp
  void *(*entry)(void *);
  void *arg;
  struct ctx_regs *regs;
  struct task *next; // run queue link
  struct list_head list;
  struct list_head rq_node;
  struct mm *mm;
  int cpu_no;
  int linked;
};

struct task *task_create(void *(*entry)(void *), void *arg);
struct task *task_link(struct task *);
struct task *task_unlink(struct task *);
struct task *task_find(uint32_t id);
uint64_t aspace_create(void);
void aspace_destroy(uint64_t pml4_phys);
void task_run(struct task *t);
void task_destroy(struct task *t);
void task_run_all(void);
void task_dump(void);
void task_init();
void task_exit();
void task_schedule(void);
void task_reap(void);
void task_reap_by_id(uint32_t);
void task_find_by_id(uint32_t id);
void task_demo(void);
extern struct task *current;
DECLARE_PERCPU(struct task*, percpu_current);
void task_selftest_early(void);
void task_selftest_irq(void);
int task_list_is_empty(void);

struct task * task_create_user(const void *, uint64_t );
#endif // TASK_H
