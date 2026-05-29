section .multiboot2
align 8
header_start:
    dd 0xe85250d6                ; magic
    dd 0                         ; i386 architecture
    dd header_end - header_start ; header length
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; checksum

    ; End tag
    dw 0                         ; type = end
    dw 0                         ; flags
    dd 8                         ; size
header_end:
