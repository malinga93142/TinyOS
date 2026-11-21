# Booting x86-64: From Multiboot2 Handoff to the Higher Half {#ch:boot}

Before a higher-half virtual memory manager or a C runtime can exist, something must load the kernel image, put the processor into a known state, and transfer control. This chapter follows that path end to end: the Multiboot2 handoff, the first 32-bit instructions, the static page tables, the switch into long mode, and the jump to the higher half. It closes with how each step fails and how to debug it.

The listings use GNU assembler (AT&T syntax) and the C preprocessor. Two macros are assumed throughout:

```c
#define KERNEL_VIRT_BASE 0xffffff8000000000
#define V2P(x)           ((x) - KERNEL_VIRT_BASE)
```

## Why Multiboot2

Writing a bootloader is a worthwhile exercise, but a standard handoff protocol removes real-mode disk code from the kernel and gives a precisely specified entry state. Multiboot2 is implemented by GRUB2, and Limine also supports it. The kernel therefore does not need its own disk or BIOS routines to be loaded by any of them.

### The state at entry

The Multiboot2 specification (section 3.3, "I386 machine state") guarantees the following when the first kernel instruction runs:

* `%eax` holds the magic value `0x36d76289`, and `%ebx` holds the *physical* address of the boot information structure.
* `CS` is a 32-bit read/execute code segment and the data segments are 32-bit read/write, all with base 0 and limit `0xFFFFFFFF`. The selector values themselves are undefined.
* The A20 gate is enabled. `CR0.PE` is set and `CR0.PG` is clear, so protected mode is on and paging is off.
* `EFLAGS.IF` and `EFLAGS.VM` are clear: interrupts are disabled.
* `%esp` is undefined; the kernel must build its own stack before it needs one.
* The GDT and IDT may be invalid, so the kernel must not load segment registers before installing its own GDT.

Note that `%eax` and `%ebx` are the *only* channel for the boot information. Whatever the entry code does next must not overwrite them before they are saved.

## The entry stub and the GDT

The entry stub saves the boot parameters, installs a GDT, and reloads the data segments:

```gas
_start:
    cli
    mov  %eax, %edi
    mov  %ebx, %esi
    lgdt (V2P(gdt_descriptor))
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    mov  %ax, %ss
```

Two design points deserve attention.

**Preserving the boot parameters.** Copying `%eax` to `%edi` and `%ebx` to `%esi` places the magic value and the boot-info pointer exactly where the System V AMD64 calling convention expects the first two arguments (`%rdi`, `%rsi`). A later `kmain(magic, info)` then receives them without further shuffling. The registers `%edi` and `%esi` must stay untouched by everything in between. Be careful with `rep stos`, which uses `%edi`, if you zero the page tables at runtime.

**The GDT.** The listings assume the following layout, which is a convention of this kernel and not a hardware requirement:

| Selector | Descriptor |
|:--------:|:-----------------------------------|
| `0x00`   | null |
| `0x08`   | 32-bit code (used before long mode) |
| `0x10`   | 32-bit data |
| `0x18`   | 64-bit code (`L=1`, `D=0`) |
| `0x20`   | 64-bit data |

Installing it first overrides whatever the loader left behind. The GDT base stored in the GDTR is a linear address, which is why the descriptor is referenced through `V2P`: paging is still off and the code runs at physical addresses.

## Static page tables

To enter long mode the CPU needs a valid 4-level hierarchy (PML4, PDPT, PD) in place *before* paging is turned on. This kernel builds it statically in the image.

### Index arithmetic

A 48-bit virtual address selects an entry at each level using 9-bit fields:
$$
\begin{aligned}
\text{PML4 index} &= \text{VA}[47{:}39],\\
\text{PDPT index} &= \text{VA}[38{:}30],\\
\text{PD index}   &= \text{VA}[29{:}21].
\end{aligned}
$$
For `0xffffff8000000000`:
$$
\texttt{0xffffff8000000000} \gg 39 \;\&\; \texttt{0x1ff} = 511.
$$
The higher-half base is therefore covered by **PML4 entry 511**. Entry 510 would start at `0xffffff0000000000`, which is a different region. Any prose or comment that says "entry 510" for this base is off by one.

### The PML4

```gas
.align 4096
pml4:
    .quad V2P(pdpt) + 0x03      # entry 0:   identity map
    .fill 510, 8, 0             # entries 1..510 unused
    .quad V2P(pdpt) + 0x03      # entry 511: 0xffffff8000000000
```

The `.fill 510` covers entries 1 to 510, so the final `.quad` lands in entry 511, which agrees with the arithmetic above. Both entries point at the *same* PDPT. Every low address `x` and the high address `KERNEL_VIRT_BASE + x` therefore translate to the same physical frame. The flag value `0x03` is `PRESENT | WRITE`.

### The PDPT and page directories

```gas
.align 4096
pdpt:
    .quad V2P(pd)  + 0x03       # VA bits [38:30] = 0 -> first 1 GiB
    .quad V2P(pd2) + 0x03       # VA bits [38:30] = 1 -> second 1 GiB
    .fill 510, 8, 0

.align 4096
pd:                              # 512 x 2 MiB = 1 GiB
    .set i, 0
    .rept 512
        .quad (i << 21) | 0x83   # PRESENT | WRITE | PAGE_SIZE
        .set i, i + 1
    .endr
pd2:
    .rept 512
        .quad (i << 21) | 0x83
        .set i, i + 1
    .endr
```

Here `0x83` is `PRESENT | WRITE | PS`. The `PS` bit (bit 7) is meaningful at the *PD* level, where it makes the entry map a 2 MiB page directly and removes the need for a page-table (PT) level. Each physical address in a PD entry must be 2 MiB aligned, which the `i << 21` form guarantees. The PDPT entries themselves use `0x03`: with `PS` set at the PDPT level an entry would instead map a 1 GiB page.

Two consequences of aliasing through a shared PDPT are worth recording. First, the identity map is only scaffolding: it exists so that the instruction stream survives the moment paging is enabled, and it should be removed once execution is in the higher half so that stray low-address accesses fault instead of silently succeeding. Second, because both views share the same lower-level tables, they also share permissions; distinct permissions for the two views (for example, a non-executable low alias) require separate tables.

## Entering long mode

Four architectural switches must be set in the right order. The sequence is as follows.

1. **`CR4.PAE`** (bit 5): required, because long mode uses the 64-bit page-table format.
2. **`CR3`**: load the *physical* address of the PML4.
3. **`EFER.LME`** (bit 8 of MSR `0xC0000080`): request long mode.
4. **`CR0.PG`** (bit 31): enabling paging while `LME` is set activates long mode (`EFER.LMA` becomes 1).

```gas
    movl %cr4, %eax
    orl  $(1 << 5), %eax        # CR4.PAE
    movl %eax, %cr4

    movl $V2P(pml4), %eax
    movl %eax, %cr3             # physical address of the PML4

    movl $0xC0000080, %ecx      # IA32_EFER
    rdmsr                       # clobbers %eax, %edx
    orl  $(1 << 8), %eax        # EFER.LME
    wrmsr

    movl %cr0, %eax
    orl  $(1 << 31), %eax       # CR0.PG: translation starts now
    movl %eax, %cr0

    ljmp $0x18, $(V2P(higher_half))
```

`rdmsr` and `wrmsr` use `%ecx`, `%eax` and `%edx`, which is why the boot parameters were moved to `%edi` and `%esi` earlier.

### What the far jump does, and what it does not

The instruction after `mov %eax, %cr0` is fetched from a *low* address, so it only survives because the identity alias exists. After paging is on and `LME` is set, the CPU is in compatibility mode, still executing 32-bit code. The `ljmp` to selector `0x18` loads the 64-bit code descriptor and enters true 64-bit mode.

However, the target `V2P(higher_half)` is a physical, low address. At this point the processor is running 64-bit code, but `RIP` is still in the low half. It reaches the higher half only with the absolute indirect jump in the 64-bit code:

```gas
.code64
higher_half:
    mov   $0x20, %ax
    mov   %ax, %ds
    mov   %ax, %es
    mov   %ax, %fs
    mov   %ax, %gs
    mov   %ax, %ss
    movl  %edi, %edi            # zero-extend the saved magic
    movl  %esi, %esi            # zero-extend the saved boot-info pointer
    movabs $stack_top, %rsp     # a high-half stack address
    movabs $jump_here, %rax     # a high-half code address
    jmp   *%rax                 # RIP moves to the higher half here
```

`movabs` is needed because these symbols have 64-bit values that do not fit a sign-extended 32-bit immediate. The two `movl` instructions are defensive. Writing a 32-bit register in 64-bit mode zero-extends into the full register, so the System V arguments are guaranteed clean regardless of what the upper halves held across the mode switch. (I did not confirm whether the architecture guarantees those upper halves are preserved across the switch, so the zero-extension removes the question at no cost.)

The linker script must support all of this. The boot code and its data must be placed at their low physical addresses (so `V2P` arithmetic holds), while the rest of the kernel gets a higher-half VMA with a low load address (LMA), typically with `AT(...)`.

## When the mapping is wrong

The classic early-boot failure is a mismatch between where the code *is* and what the page tables *map*. Typical causes are a forgotten identity map, a `V2P` applied to the wrong symbol, a stack or jump target left at a low address after the low map has been dropped, or a table that is not 4 KiB aligned.

The mechanism is mechanical. If `RIP` points at an address with no valid translation, the instruction fetch raises a page fault (`#PF`). Delivering that fault needs an IDT entry. At this stage no valid IDT exists, so delivery fails and the CPU raises a double fault (`#DF`). Delivery of the double fault fails in the same way, and the processor shuts down with a **triple fault**, which a virtual machine reports as a reset.

Alignment is not the issue as such. What matters is that the fetched address must be mapped, and that the physical frame must contain the code you expect.

### Debugging

QEMU can show the first fault instead of silently rebooting:

```sh
qemu-system-x86_64 -kernel kernel.elf -d int,cpu_reset -no-reboot
```

`-d int` logs each exception with its vector, error code and `CR2` (the faulting address), and `-no-reboot` stops the machine at the triple fault rather than restarting it. Reading the first `#PF` entry and comparing `CR2` and `RIP` with the linker map usually identifies the broken translation immediately.

## Summary

The whole boot path hangs on one invariant: **at every instruction boundary, `RIP`, `RSP` and every memory operand must be mapped.** Before paging that means physical addresses; at the moment paging turns on it means the identity alias; after the `jmp *%rax` it means the higher-half alias; and only then may the identity mapping be discarded.

| Step | Mechanism | Addresses in use |
|:-----|:----------|:-----------------|
| Handoff | Multiboot2, `%eax`/`%ebx` | physical |
| GDT, save args | `lgdt`, `%edi`/`%esi` | physical |
| Enable paging | `PAE`, `CR3`, `LME`, `PG` | physical via identity alias |
| `ljmp $0x18` | enter 64-bit mode | still low (identity alias) |
| `jmp *%rax` | `movabs` absolute jump | higher half (PML4 entry 511) |
