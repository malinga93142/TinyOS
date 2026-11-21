.section .rodata
.global hello_start, hello_end
hello_start:
    .incbin "build/user/hello.bin"
hello_end:
