CC = gcc
AS = nasm

CFLAGS = -ffreestanding -m32 -g -O0 -Wall -Wextra -I.
LDFLAGS = -T linker.ld -m32 -nostdlib -lgcc

C_SRC = \
	kernel.c \
	fs/vfs.c \
	fs/fat32.c \
	drivers/vga.c \
	drivers/keyboard.c \
	drivers/mouse.c \
	drivers/ps2.c \
	drivers/ata.c \
	drivers/pic.c \
	drivers/pit.c \
	interrupts/idt.c \
	gdt/gdt.c

C_OBJ = $(C_SRC:.c=.o)

ASM_SRC = \
	boot.asm \
	interrupts/isr.asm \
	gdt/gdt.asm

ASM_OBJ = \
	boot.o \
	interrupts/isr.o \
	gdt/gdt_asm.o

OBJ = $(C_OBJ) $(ASM_OBJ)

all: kernel.bin iso diskimg

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

boot.o: boot.asm
	$(AS) -f elf32 $< -o $@

interrupts/isr.o: interrupts/isr.asm
	$(AS) -f elf32 $< -o $@

gdt/gdt_asm.o: gdt/gdt.asm
	$(AS) -f elf32 $< -o $@

kernel.bin: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

iso: kernel.bin grub.cfg
	rm -rf isodir
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub2-mkrescue -o turbOS.iso isodir

diskimg:
	@test -f turbOS-disk.img || { \
		truncate -s 64M turbOS-disk.img; \
		mkfs.fat -F 32 -n TURBOS turbOS-disk.img; \
	}

run: all
	qemu-system-i386 \
		-boot order=d \
		-cdrom turbOS.iso \
		-drive file=turbOS-disk.img,format=raw,if=ide \
		-m 512M

clean:
	rm -f $(OBJ)
	rm -f kernel.bin
	rm -f turbOS.iso
	rm -rf isodir
