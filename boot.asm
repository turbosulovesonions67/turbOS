section .multiboot2
align 8
header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))
    dw 0
    dw 0
    dd 8
header_end:

section .text
global _start
extern kernel_main
extern stack_top

_start:
    cli
    mov esp, stack_top
    and esp, 0xfffffff0
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
