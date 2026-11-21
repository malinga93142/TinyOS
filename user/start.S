.text
.global _start
_start:
    call main
    mov  %eax, %edi
    mov  $0, %eax
    syscall
1:  jmp 1b
