bits 64

section .text
global _start
extern kernel_main

_start:
    cli

    mov rsp, stack_top
    and rsp, -16

    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
