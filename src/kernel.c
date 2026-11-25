#include "../include/acpi.h"
#include "../include/ahci.h"
#include "../include/ata.h"
#include "../include/cpu.h"
#include "../include/cpu_features.h"
#include "../include/gdt.h"
#include "../include/gfx.h"
#include "../include/hpet.h"
#include "../include/idt.h"
#include "../include/kbd.h"
#include "../include/kmalloc.h"
#include "../include/lapic.h"
#include "../include/mb.h"
#include "../include/misc.h"
#include "../include/mouse.h"
#include "../include/paging.h"
#include "../include/pci.h"
#include "../include/percpu.h"
#include "../include/pic.h"
#include "../include/pmm.h"
#include "../include/printk.h"
#include "../include/smp.h"
#include "../include/syscall.h"
#include "../include/task.h"
#include "../include/uart.h"
#include <stdatomic.h>
#include <stdint.h>

#define IPI_WAKE_VECTOR 0xFD
#define TIMER_HZ 1000

struct framebuffer fb;
struct cpu_topology topo;
_Atomic int kernel_ready;
_Atomic int ap_halt;

extern uint8_t kernel_start[];
extern uint8_t kernel_end[];
extern void setupfsbase(void);
extern void test();
static uint32_t lapic_id_bsp;
static int x2_enabled;
static void fb_map(void) {
 uint64_t pa = fb.addr & ~0xFFFULL;
 uint64_t end = fb.addr + (uint64_t)fb.pitch * fb.height;
 for (; pa < end; pa += PAGE_SIZE)
  paging_map_page((uint64_t)(uintptr_t)PA_TO_VA(pa), pa,
                  PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
 fb.addr = (uint64_t)(uintptr_t)PA_TO_VA(fb.addr);
}
static void early_init(uint32_t mb_info_ptr) {
 mb_parse((struct mb_info *)(uintptr_t)mb_info_ptr, &fb);
 // kprintf("[FB] addr=%lx %ux%u pitch=%u\n", (uint64_t)fb.addr, fb.width, fb.height, fb.pitch);
 uart_init();
 percpu_init(0, 0, 1);
 // kprintf("[CPU %u] PERCPU INIT\n", cpu_id());
 init_gdt();
 init_idt();
 init_pics(0x20, 0x28);
 // kprintf("[KERNEL] EARLY BOOT INITIALIZATION\n");
}

static void memory_init(void) {
 uint64_t kphys_start = (uint64_t)(uintptr_t)(kernel_start - KERNEL_BASE);
 uint64_t kphys_end = (uint64_t)(uintptr_t)(kernel_end - KERNEL_BASE);
 pmm_init();
 paging_init(kphys_start, kphys_end);
}

static void cpu_features_init(void) {
 print_cr3();
 if (pcid_support() == -1)
  kprintf("[KERNEL] No PCID Support\n");
 if (invpcid_support() == -1)
  kprintf("[KERNEL] No INVPCID Support\n");
 init_cpu_extended_states();
 enable_fsbase_in_cr4();
 print_cr4();
}

static void hpet_init_from(struct acpi_sdt_header *hdr) {
 struct acpi_hpet *hpet = (struct acpi_hpet *)hdr;
 uint64_t pa = hpet->address.address;
 kprintf("[KERNEL] HPET Base Address: 0x%lx\n", pa);
 kprintf("[KERNEL] HPET PA_VA_MMIO: %lx\n", (uint64_t)(uintptr_t)PA_TO_VA(pa));
 // paging_map_page((uint64_t)PA_TO_VA(pa), pa, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
 paging_map_page((uint64_t)PA_TO_VA(pa), pa, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
 setup_hpet((uint64_t)(uintptr_t)PA_TO_VA(pa));
}

static void acpi_init(uintptr_t mb_info_ptr) {
 void *rsdp = mb_find_acpi_rsdp((struct mb_info *)mb_info_ptr);
 if (!rsdp)
  return;
 int is_xsdt;
 struct acpi_sdt_header *root = acpi_get_root_table(rsdp, &is_xsdt);
 struct acpi_sdt_header *madt = acpi_find_table(root, is_xsdt, "APIC");
 if (madt) {
  acpi_parse_madt((struct acpi_madt *)madt, &topo);
  kprintf("[KERNEL] CPU COUNT= %u\n", topo.cpu_count);
  for (uint32_t i = 0; i < topo.cpu_count; i++)
   kprintf("[KERNEL] APIC ID[%u]= %u\n", i, topo.apic_ids[i]);
 }
 struct acpi_sdt_header *hpet = acpi_find_table(root, is_xsdt, "HPET");
 if (hpet)
  hpet_init_from(hpet);
}

static void lapic_boot_init(void) {
 if (x2apic_supported()) {
  kprintf("[KERNEL] x2APIC is supported\n");
  lapic_init_x2();
  lapic_id_bsp = lapic_get_id_x2();
  x2_enabled = 1;
 } else {
  kprintf("[KERNEL] LAPIC is supported\n");
  lapic_init_mmio();
  lapic_id_bsp = lapic_get_id_mmio();
 }
 kprintf("[KERNEL] LAPIC INIT\n");
 lapic_lvt_init();
 lapic_timer_calibrate();
}

static void smp_start_aps(void) {
 for (uint32_t i = 0; i < topo.cpu_count; i++)
  if (topo.apic_ids[i] != lapic_id_bsp)
   smp_bringup(topo.apic_ids[i]);
}

static void ata_boot_init(void) {
 ata_init();
 kprintf("[KERNEL] ATA INIT\n");
 for (int i = 0; i < 4; i++)
  if (ata_drives[i].present)
   kprintf("ata%d: %s, %lu sectors, lba48=%d\n", i, ata_drives[i].model,
           ata_drives[i].sectors, ata_drives[i].lba48);
}

static void wait_ap_halt(void) {
 if (topo.cpu_count > 1)
  while (!atomic_load_explicit(&ap_halt, memory_order_acquire))
   ;
}

static void wake_ap(void) {
 if (x2_enabled && topo.cpu_count > 1)
  lapic_send_ipi_x2(topo.apic_ids[1], IPI_WAKE_VECTOR);
}
static void console_loop(void) {
 struct key_event ev;
 for (;;) {
  mouse_poll();
  while (kbd_poll(&ev))
   if (ev.pressed &&
       ((ev.ascii >= 32 && ev.ascii < 127) || ev.ascii == '\n' ||
        ev.ascii == '\b' || ev.ascii == '\t'))
    kprintf("$%c", ev.ascii);
     // putc_fb(ev.ascii);
  // uint8_t st = inb(0x64);
  // if (st & 1) {
  // 	uint8_t b = inb(0x60);
  //  kprintf("[MOUSE] st=%x b=%x\n", st,b);
  // 	// if (b)
  // }
  asm volatile("hlt");
 }
}
int activate_fb_from_here = 0;
void kmain(uint32_t mb_magic, uint32_t mb_info_ptr) {
 (void)mb_magic;
 early_init(mb_info_ptr);
 // kprintf("\n[KERNEL] KERNEL STARTED\n");
 memory_init();
 acpi_init((uintptr_t)mb_info_ptr);
 fb_map();
 activate_fb_from_here = 1;
 fb_init(&fb);
 cpu_features_init();
 lapic_boot_init();
 kbd_init();
 // mouse_init(fb.width, fb.height);
 // mouse_selftest();
 smp_start_aps();
 kprintf("[KERNEL] KMALLOC INIT\n");
 kmalloc_init();
 ata_boot_init();
 pci_enumerate_bus();
 task_init();
 syscall_init();
 // task creation
 extern const uint8_t hello_start[], hello_end[];
 task_create_user(hello_start, hello_end - hello_start);
 unmap_lower_half();
 setupfsbase();
 kprintf("[KERNEL] FRAMES= %lu\n", (unsigned long)pmm_free_frames());
 asm volatile("sti");
 lapic_timer_start_periodic(TIMER_HZ);
 atomic_store_explicit(&kernel_ready, 1, memory_order_release);
 kprintf("[KERNEL] FRAMES= %lu\n", (unsigned long)pmm_free_frames());
 wait_ap_halt();
 wake_ap();
 // test();
 task_schedule();
 // console_loop();
 // shell();
}
