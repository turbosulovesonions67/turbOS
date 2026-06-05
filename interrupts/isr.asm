global isr0
global irq0

extern isr0_handler
extern irq0_handler

section .text

isr0:
    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr0_handler

    pop gs
    pop fs
    pop es
    pop ds

    popa

    iret

irq0:
    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call irq0_handler

    pop gs
    pop fs
    pop es
    pop ds

    popa

    iret
