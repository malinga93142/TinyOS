// void switch_to(struct task *prev (rdi), struct task *next (rsi))
// struct task: saved_rsp at 0, cr3 at 8
.global switch_to
switch_to:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq  %rsp, (%rdi)        # prev->saved_rsp

    movq  8(%rsi), %rax       # next->cr3
    movq  %cr3, %rcx
    cmpq  %rax, %rcx
    je    1f
    movq  %rax, %cr3          # stacks are under PML4[511], so RSP stays valid
1:
    movq  (%rsi), %rsp        # next->saved_rsp
    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %rbp
    popq  %rbx
    ret
.global user_enter
user_enter:
    iretq
