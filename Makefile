CC = gcc
AS = nasm
CFLAGS = -ffreestanding -m32 -g -O0 -Wall -Wextra
LDFLAGS = -T linker.ld -m32 -nostdlib -lgcc

all: kernel.bin iso

boot.o: boot.asm
	$(AS) -f elf32 boot.asm -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.bin: boot.o kernel.o
	$(CC) $(LDFLAGS) boot.o kernel.o -o kernel.bin

iso:
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub2-mkrescue -o turbOS.iso isodir

clean:
	rm -rf *.o kernel.bin turbOS.iso isodir

run:
	qemu-system-i386 -cdrom turbOS.iso -m 512M
