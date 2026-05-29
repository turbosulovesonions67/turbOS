ENTRY(kernel_main)

SECTIONS
{
    . = 0x100000;

    /* Put multiboot header at the VERY beginning */
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
}
