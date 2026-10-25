section .multiboot2
align 8
header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd -(0xe85250d6 + (header_end - header_start))

    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32
    dd 0

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
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
