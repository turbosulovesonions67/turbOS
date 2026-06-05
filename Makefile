CC = gcc
AS = nasm

CFLAGS = -ffreestanding -m32 -g -O0 -Wall -Wextra
LDFLAGS = -T linker.ld -m32 -nostdlib -lgcc

all: kernel.bin iso

boot.o: boot.asm
	$(AS) -f elf32 boot.asm -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

drivers/ata.o: drivers/ata.c drivers/ata.h
	$(CC) $(CFLAGS) -c drivers/ata.c -o drivers/ata.o

drivers/pic.o: drivers/pic.c drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/pic.c -o drivers/pic.o

drivers/pit.o: drivers/pit.c drivers/pit.h
	$(CC) $(CFLAGS) -c drivers/pit.c -o drivers/pit.o

interrupts/idt.o: interrupts/idt.c interrupts/idt.h interrupts/isr.h
	$(CC) $(CFLAGS) -c interrupts/idt.c -o interrupts/idt.o

interrupts/isr.o: interrupts/isr.asm
	$(AS) -f elf32 interrupts/isr.asm -o interrupts/isr.o

gdt/gdt.o: gdt/gdt.c gdt/gdt.h
	$(CC) $(CFLAGS) -c gdt/gdt.c -o gdt/gdt.o

gdt/gdt_asm.o: gdt/gdt.asm
	$(AS) -f elf32 gdt/gdt.asm -o gdt/gdt_asm.o

kernel.bin: \
	boot.o \
	kernel.o \
	drivers/ata.o \
	drivers/pic.o \
	drivers/pit.o \
	interrupts/idt.o \
	interrupts/isr.o \
	gdt/gdt.o \
	gdt/gdt_asm.o
	$(CC) $(LDFLAGS) \
	boot.o \
	kernel.o \
	drivers/ata.o \
	drivers/pic.o \
	drivers/pit.o \
	interrupts/idt.o \
	interrupts/isr.o \
	gdt/gdt.o \
	gdt/gdt_asm.o \
	-o kernel.bin

iso:
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub2-mkrescue -o turbOS.iso isodir

clean:
	rm -rf *.o
	rm -rf drivers/*.o
	rm -rf interrupts/*.o
	rm -rf gdt/*.o
	rm -rf kernel.bin
	rm -rf turbOS.iso
	rm -rf isodir

run:
	qemu-system-i386 -cdrom turbOS.iso -m 512M
