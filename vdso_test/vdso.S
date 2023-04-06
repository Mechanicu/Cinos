    .globl vdso_start
    .globl vdso_end
    .section .rodata
    .balign 4096
vdso_start:
    .incbin "./vdso.so"
    .balign 4096
vdso_end: