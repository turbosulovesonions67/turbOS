## DO NOT LISTEN TO RELEASES >:(((((

# turbOS

**An experimental 32-bit x86 operating system written in C and assembly.**

turbOS is a hobby operating system built from scratch to explore kernel development, hardware interfaces, persistent storage, graphics, and system-level programming. It boots through GRUB using the Multiboot2 protocol and is developed and tested primarily in QEMU.

The project includes a freestanding kernel, hardware drivers, a FAT32-backed Virtual Filesystem (VFS), built-in applications, graphical interface components, and an experimental programming language toolchain.

Still a work in progress. Still being built. Still refusing to listen to releases.

## Features

### Kernel and Hardware

- 32-bit x86 kernel
- GRUB bootloader with Multiboot2 support
- Assembly boot entry point and custom linker script
- Global Descriptor Table (GDT)
- Interrupt Descriptor Table (IDT) and interrupt service routines
- Programmable Interrupt Controller (PIC)
- Programmable Interval Timer (PIT)
- Real-time clock (RTC)
- VGA text-mode output
- Keyboard and PS/2 mouse support
- ATA PIO disk access
- PC speaker support
- Framebuffer driver and graphics-related components
- Basic power-management routines

### Storage and Filesystems

turbOS implements its own storage and filesystem components, with ATA disk access underneath a FAT32 filesystem and Virtual Filesystem (VFS) layer.

- ATA sector reads and writes
- FAT32 filesystem implementation
- Virtual Filesystem abstraction
- File and directory management
- Virtual disk image for persistent-storage experiments
- Filesystem integration with the terminal, editor, and file manager

The filesystem is still experimental. Supported operations, filename handling, storage limits, and compatibility depend on the current implementation.

### User Interface

The project includes several built-in interfaces and applications:

- **Home** — main system interface
- **Terminal** — command-line environment
- **File manager** — browse and manage files
- **Text editor** — edit and save text files
- **Pong** — built-in game
- **Screen saver** — screen-saver component
- **GUI** — desktop, graphics routines, icons, and font resources

The project contains both VGA text-mode and framebuffer/graphics-related code. These components are still evolving, and not every interface necessarily uses the same rendering path.

### Accounts

The kernel includes an account-management component in `kernel/accounts.c` and `kernel/accounts.h`, intended to support the system's account-related functionality.

### Turbix and TEX

**Turbix** is an experimental compiler for the `.tix` source format. It produces `.tex` bytecode, which is handled by the TEX runtime.

A minimal example using the documented syntax:

```text
program start
main():
    text("Hello from turbOS")
    loop end
program end
```

The compiler and runtime are located in `compiler/` and `runtime/`.

Turbix and TEX are works in progress. Their syntax, instruction set, bytecode format, and supported operations may change as development continues.

## Requirements

The project uses a freestanding 32-bit GCC build, NASM for assembly, GRUB utilities for ISO generation, and QEMU for emulation.

On Fedora, install the principal tools with:

```bash
sudo dnf install gcc make nasm grub2-tools \
    grub2-tools-extra xorriso qemu-system-x86 dosfstools
```

Additional toolchain components may be needed depending on your Fedora installation.

## Build and Run

Clone the repository:

```bash
git clone https://github.com/turbosulovesonions67/turbOS.git
cd turbOS
```

Build the kernel, ISO, and virtual disk image:

```bash
make
```

Build and launch turbOS in QEMU:

```bash
make run
```

The current Makefile configures QEMU with:

- `qemu-system-i386`
- 512 MiB of emulated RAM
- A bootable ISO image
- An IDE-attached raw disk image
- User-mode networking and an emulated RTL8139 network adapter

The network adapter is part of the virtual machine configuration; its presence does not imply that a network stack is implemented in turbOS.

### Build Configuration

The kernel is compiled using GCC with the following principal flags:

```text
-ffreestanding -m32 -g -O0 -Wall -Wextra -I.
```

The linker uses `linker.ld`, with `-nostdlib` and `-lgcc`. Assembly files are assembled in ELF32 format using NASM.

The boot configuration loads `kernel.bin` through GRUB's Multiboot2 interface. The boot header also requests a 1024 × 768 × 32 framebuffer mode.

### Generated Files

| File | Purpose |
|---|---|
| `kernel.bin` | Linked kernel binary |
| `turbOS.iso` | Bootable ISO |
| `turbOS-disk.img` | Virtual FAT32 disk |
| `isodir/` | Temporary ISO staging directory |
| `*.o` | Compiled object files |

The disk-image target creates a 64 MiB FAT32 image labelled `TURBOS` only if the image does not already exist. Existing disk images are reused between runs.

### Cleaning Build Artifacts

```bash
make clean
```

This removes compiled object files, `kernel.bin`, `turbOS.iso`, and the `isodir/` directory. It preserves the virtual disk image.

## Working With Files

The filesystem components are designed to provide file and directory access through the VFS and FAT32 layers. The terminal, file manager, and text editor provide interfaces for working with files.

Because the filesystem is still under development, avoid treating it as a fully compatible or production-ready storage implementation. Back up any virtual disk data you care about before experimenting with filesystem changes.

## Compiling a TEX Program

Create a source file with the `.tix` extension using the editor. For example, save your program as `pg.tix`.

Compile it from the turbOS terminal:

```text
turbix -c pg.tix
```

Execute the generated bytecode:

```text
run pg.tex
```

Compilation and execution are separate operations. The compiler generates the bytecode file, and the runtime interprets the supported instructions. Exact language and runtime capabilities depend on the current implementation.

## Project Structure

```text
turbOS/
├── compiler/       # Turbix compiler
├── drivers/        # ATA, framebuffer, input, VGA, speaker, etc.
├── fs/             # FAT32 and VFS
├── gdt/            # GDT implementation and assembly
├── gui/            # Desktop, graphics, icons, font, memory utilities
├── interrupts/     # IDT and interrupt service routines
├── kernel/         # Kernel main, state, accounts
├── power/          # Power-related routines
├── runtime/        # TEX bytecode runtime
├── time/           # RTC
├── ui/             # Home, terminal, editor, file manager, Pong, saver
├── boot.asm        # Multiboot2 header and boot entry
├── kernel.c        # Additional kernel source
├── linker.ld       # Kernel linker script
├── grub.cfg        # GRUB boot configuration
├── Makefile        # Build system
├── LICENSE
└── README.md
```

The tree highlights the main components. Supporting headers, assembly routines, and resources are kept in their respective directories.

## Development Status

turbOS is an actively developed hobby operating system. Its purpose is experimentation, learning, and exploring low-level computing — not general-purpose computing at this stage.

Hardware support is limited to implemented drivers. Filesystem behavior, graphical components, and the compiler/runtime are evolving. The presence of a component in the source tree does not guarantee that every part is fully integrated or feature-complete.

Test changes in QEMU before experimenting with physical hardware or storage devices.

## Contributing

Suggestions, bug reports, and technical contributions are welcome.

- Keep changes focused on the relevant subsystem.
- Build and test changes in QEMU.
- Check filesystem modifications for persistence across restarts.
- Test compiler and runtime changes together.
- Include relevant build output and reproduction steps when reporting bugs.

The project is under active development, so clear documentation and reproducible testing are appreciated.

## License

See [`LICENSE`](LICENSE) for the applicable license terms.

## Credits

**Turbosu Pramanik © 2026**

An independent hobby OS built through experimentation, debugging, and a refusal to leave low-level computing alone.

**Long live turbOS. Long live `##NOAI`.**
<<<<<<< HEAD
=======

>>>>>>> 83c2dfb (gui/desktop.c checking (LLM assisted of) completed so a few tweaks with LLM and cleanup and then pushed1)
