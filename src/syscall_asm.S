.global syscall_entry
syscall_entry:
    movq  %rsp, syscall_usp(%rip)
    movq  syscall_ksp(%rip), %rsp
    pushq syscall_usp(%rip)
    pushq %r11
    pushq %rcx
    pushq $0
    movq  %r8, %r9
    movq  %r10, %r8
    movq  %rdx, %rcx
    movq  %rsi, %rdx
    movq  %rdi, %rsi
    movq  %rax, %rdi
    call  syscall_handler
    addq  $8, %rsp
    popq  %rcx
    popq  %r11
    popq  %rsp
    sysretq
