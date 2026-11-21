#include "../include/cpu.h"
#include "../include/hpet.h"
#include "../include/paging.h"
#include "../include/percpu.h"
#include "../include/pmm.h"
#include "../include/printk.h"
#include "../include/vmm.h"
#include <stdint.h>
struct __attribute__((packed)) dtr {
  uint16_t limit;
  uint64_t base;
};
struct dtr g, i;
void store() {
  asm volatile("sgdt %0" : "=m"(g));
  asm volatile("sidt %0" : "=m"(i));
  kprintf("[KERNEL] GDT base=%x IDT base=%x RSP=%x\n", g.base, i.base,
          (uint64_t)__builtin_frame_address(0));
  asm volatile("lgdt %0" ::"m"(g));
  asm volatile("lidt %0" ::"m"(i));
}
#define TEST_VA 0x8000000000ULL /* PML4 idx 1, PDPT 0, PD 0, PT 0 */

static uint64_t new_pml4[512] __attribute__((aligned(4096)));
static uint64_t new_pdpt[512] __attribute__((aligned(4096)));
static uint64_t new_pd[512] __attribute__((aligned(4096)));
static uint64_t new_pt[512] __attribute__((aligned(4096)));
static uint8_t test_page[4096] __attribute__((aligned(4096)));

void paging_switch(void) {
  uint64_t old = getcr3(); /* 0xXXX000 */
  uint64_t *old_pml4 = (uint64_t *)(KERNEL_BASE + (old & ~0xFFFULL));

  for (int i = 0; i < 512; i++) /* share kernel mappings */
    new_pml4[i] = old_pml4[i];
  kprintf("[PAGING SWITCH] PML[511] = %x\n", old_pml4[511]);
  if (new_pml4[1]) {
    kprintf("[PAGING SWITCH] PML4[1] already used\n");
    return;
  }

  new_pt[0] = VA_TO_PA(test_page) | PAGE_PRESENT | PAGE_WRITE;
  new_pd[0] = VA_TO_PA(new_pt) | PAGE_PRESENT | PAGE_WRITE;
  new_pdpt[0] = VA_TO_PA(new_pd) | PAGE_PRESENT | PAGE_WRITE;
  new_pml4[1] = VA_TO_PA(new_pdpt) | PAGE_PRESENT | PAGE_WRITE;
  new_pml4[2] = VA_TO_PA(new_pdpt) | PAGE_PRESENT | PAGE_WRITE;
  // new_pml4[511]=VA_TO_PA(new_pdpt)  | PAGE_PRESENT | PAGE_WRITE;
  setcr3(VA_TO_PA(new_pml4));
  kprintf("[PAGING SWITCH] NEW PAGE LOADED CR3=%x\n", getcr3());
  *(volatile uint64_t *)TEST_VA = 0xdeadbaba; /* only valid here */
  kprintf("[PAGING SWITCH] TEST_VA: %x\n", *(volatile uint64_t *)TEST_VA);
  setcr3(old); /* back to 0xXXX000 */
  kprintf("[PAGING SWITCH] SWITCH BACK CR3=%x TEST_PAGE[0]=%x\n", getcr3(),
          *(volatile uint64_t *)test_page); /* deadbeef: same frame */
  // *(volatile uint64_t *)TEST_VA = 0xdeadbaba;
}
void mapping_test() {
  uint64_t pa = paging_virt_to_phys(0xffffff800d600000);
  kprintf("[KERNEL] VA: %x, PA: %x\n", 0xffffff800d600000, pa);
}
void vmm_test() {
  uint64_t *mem = vmalloc(10);
  *mem = 1;
  mem[4096 / 8] = 2;
  kprintf("[VMM] mem= %x\n", mem);
  kprintf("[KERNEL] FREE FRAMES: %d\n", pmm_free_frames());
  vfree(mem);
  kprintf("[KERNEL] FREE FRAMES: %d\n", pmm_free_frames());
}
void kmalloc_test() {
  struct node {
    int data;
    struct node *next;
  };
  struct node *node1 = kmalloc(sizeof(struct node));
  kprintf("[KMALLOC] NODE1= %x\n", node1);
  struct node *node2 = kmalloc(sizeof(struct node));
  kprintf("[KMALLOC] NODE2= %x\n", node2);
  node1->data = 0x10;
  node2->data = 0x20;
  node1->next = node2;
  node2->next = 0;
  kfree(node1);
  kfree(node2);
}
void some_other_(void) {
  if (cpu_id() == 0) {
    // hpet_delay_us(2000);
    // paging_switch();
    // kmem_ref_test();
    // kmem_ref_test2();
    // vmm_test();
  }
}
