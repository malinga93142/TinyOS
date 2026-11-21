.intel_syntax noprefix

.text
.globl setupfsbase

setupfsbase:
 mov ecx, 0xC0000100
 mov eax, 0xFFFF0000
 xor edx, edx
 wrmsr
 ret
