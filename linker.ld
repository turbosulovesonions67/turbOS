ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .boot : {
        *(.multiboot2)
    }

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(COMMON)
        *(.bss)
    }

    . = ALIGN(16);
    stack_bottom = .;
    . += 16384;
    stack_top = .;
}
