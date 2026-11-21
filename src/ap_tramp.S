.intel_syntax noprefix
.code16
.section .text
.global ap_trampoline_start
.global ap_trampoline_end
ap_trampoline_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    lgdt [ap_gdt_descriptor - ap_trampoline_start + 0x8000]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    ljmp 0x08, 0x8000 + (ap_protected - ap_trampoline_start)

.code32
ap_protected:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov eax, [0x8000 + (ap_cr3_val - ap_trampoline_start)]
    mov cr3, eax
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ljmp 0x18, 0x8000 + (ap_long_mode - ap_trampoline_start)
.code64
ap_long_mode:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    lidt [0x8000 + (ap_idt_ptr - ap_trampoline_start)]
    mov rax, qword ptr [0x8000 + (ap_stack_val - ap_trampoline_start)]
    mov rsp, rax
    mov rbx, qword ptr [0x8000 + (ap_main_addr - ap_trampoline_start)]
    call rbx
.hang:
    cli
    hlt
    jmp .hang
.align 8
ap_gdt_start:
    .quad 0x0000000000000000   # 0x00 null
    .quad 0x00cf9a000000ffff   # 0x08 32-bit code (G=1, D=1, L=0)
    .quad 0x00cf92000000ffff   # 0x10 data
    .quad 0x00af9a000000ffff   # 0x18 64-bit code (G=1, D=0, L=1)
ap_gdt_end:
ap_gdt_descriptor:
    .word ap_gdt_end - ap_gdt_start - 1
    .long ap_gdt_start - ap_trampoline_start + 0x8000
.global ap_cr3_val
ap_cr3_val:      .long 0
.global ap_stack_val
ap_stack_val:    .quad 0    # Updated to .quad for 64-bit pointers
.global ap_main_addr
ap_main_addr:    .quad 0    # Updated to .quad for higher-half virtual addresses
.globl ap_idt_ptr
ap_idt_ptr:
    .word 0
    .quad 0                 # Updated to .quad for 64-bit IDT base
ap_trampoline_end:
