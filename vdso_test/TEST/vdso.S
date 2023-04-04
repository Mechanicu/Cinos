    #include "vdso.h"
    .globl vdso_start
    .globl vdso_end
    // .section .rodata
    .balign PAGE_SIZE
vdso_start:
    .incbin "./vdso.so"
    .balign PAGE_SIZE
vdso_end: