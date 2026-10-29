#include "kernel/state.h"
#include "kernel/accounts.h"

#include "gdt/gdt.h"
#include "interrupts/idt.h"

#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/ps2.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "drivers/vga.h"

#include "fs/vfs.h"
#include "fs/fat32.h"

#include "time/rtc.h"
#include "power/power.h"

#include "ui/home.h"
#include "ui/saver.h"
#include "ui/editor.h"
#include "ui/terminal.h"
#include "ui/filemanager.h"
#include "ui/pong.h"

#include "gui/gfx.h"
#include "gui/desktop.h"

static void boot_wait(unsigned int ticks)
{
    unsigned int start = timer_ticks;

    while(timer_ticks - start < ticks)
        __asm__ volatile("hlt");
}

static void boot_status(const char *text, int ok)
{
    vga_print(
        text,
        boot_line,
        2,
        0x07);

    vga_print(
        ok ? "[ OK ]" : "[FAIL]",
        boot_line,
        70,
        ok ? 0x0A : 0x0C);

    boot_line++;
}

static void boot_screen(void)
{
    vga_clear(0x00);

    vga_print(
        "turbOS",
        2,
        3,
        0x0A);

    vga_print(
        "TASK-32bit operating system",
        3,
        3,
        0x07);

    vga_print(
        "Initializing system...",
        5,
        3,
        0x0F);

    boot_line = 7;
}

static void kernel_panic_disk(void)
{
    vga_clear(0x40);

    vga_print(
        "turbOS KERNEL PANIC",
        4,
        27,
        0x4F);

    vga_print(
        "A fatal error occurred while initializing storage.",
        7,
        12,
        0x4F);

    vga_print(
        "ERROR: BOOT DEVICE NOT FOUND",
        10,
        22,
        0x4F);

    vga_print(
        "ATA        [ FAIL ]",
        13,
        25,
        0x4F);

    vga_print(
        "FAT32      [ FAIL ]",
        14,
        25,
        0x4F);

    vga_print(
        "VFS        [ STOP ]",
        15,
        25,
        0x4F);

    vga_print(
        "System halted.",
        18,
        32,
        0x4F);

    vga_print(
        "Press Ctrl+Alt+Del to reboot.",
        21,
        25,
        0x4F);

    for(;;)
        __asm__ volatile("hlt");
}

static void process_global_key(int key)
{
    if(key == KEY_CTRL)
    {
        ctrl_pressed = 1;
        return;
    }

    if(!ctrl_pressed)
        return;

    if(current_screen == SCREEN_EDITOR &&
       (key == 's' || key == 'S'))
    {
        editor_save();
        return;
    }

    if(key == '1')
    {
        current_screen = SCREEN_HOME;
        draw_home();
    }
    else if(key == '2')
    {
        current_screen = SCREEN_SAVER;
        draw_saver();
    }
    else if(key == '3')
    {
        current_screen = SCREEN_EDITOR;
        draw_editor();
    }
    else if(key == '4')
    {
        current_screen = SCREEN_PONG;
        draw_pong();
    }
    else if(key == '5')
    {
        current_screen = SCREEN_TERMINAL;
        draw_terminal();
    }
    else if(key == '6')
    {
        current_screen = SCREEN_FILEMANAGER;
        draw_file_manager();
    }
    else if(key == '9')
    {
        power_reboot();
    }
    else if(key == '0')
    {
        power_shutdown();
    }
}

#define MULTIBOOT2_BOOT_MAGIC 0x36d76289U

unsigned int memory_total_mib = 0;
int memory_map_available = 0;

static unsigned int multiboot2_memory_total(unsigned int info_addr)
{
    unsigned int total_size;
    unsigned int offset;
    unsigned int total_mib = 0;

    if(!info_addr)
        return 0;

    total_size = *(volatile unsigned int *)info_addr;
    if(total_size < 16)
        return 0;

    offset = 8;
    while(offset + 8 <= total_size)
    {
        unsigned int *tag =
            (unsigned int *)(info_addr + offset);
        unsigned int type = tag[0];
        unsigned int size = tag[1];

        if(size < 8 || size > total_size - offset)
            return 0;

        if(type == 0)
            break;

        if(type == 6 && size >= 16)
        {
            unsigned int entry_size = tag[2];
            unsigned int pos = offset + 16;

            if(entry_size < 24)
                return 0;

            while(pos + entry_size <= offset + size)
            {
                unsigned int *entry =
                    (unsigned int *)(info_addr + pos);
                unsigned int length_low = entry[2];
                unsigned int length_high = entry[3];
                unsigned int region_mib;

                if(entry[4] == 1)
                {
                    if(length_high > 0x000FFFFFU)
                        return 0;

                    region_mib =
                        (length_high << 12) |
                        (length_low >> 20);

                    if(region_mib > 0xFFFFFFFFU - total_mib)
                        total_mib = 0xFFFFFFFFU;
                    else
                        total_mib += region_mib;
                }
                pos += entry_size;
            }

            return total_mib;
        }

        offset = (offset + size + 7U) & ~7U;
    }

    return 0;
}

static void multiboot_memory_init(unsigned int magic,
                                  unsigned int info_addr)
{
    memory_total_mib = 0;
    memory_map_available = 0;

    if(magic != MULTIBOOT2_BOOT_MAGIC)
        return;

    memory_total_mib = multiboot2_memory_total(info_addr);
    if(memory_total_mib)
        memory_map_available = 1;
}

void kernel_main(unsigned int multiboot_magic,
                  unsigned int multiboot_info_addr)
{
    multiboot_memory_init(multiboot_magic, multiboot_info_addr);
    unsigned char second;
    int filesystem_ok;

    gfx_init_multiboot(multiboot_magic, multiboot_info_addr);

    boot_screen();

    gdt_init();
    boot_status("Global Descriptor Table", 1);

    idt_init();
    boot_status("Interrupt Descriptor Table", 1);

    pic_remap();
    boot_status("Programmable Interrupt Controller", 1);

    pit_init(100);
    boot_status("Programmable Interval Timer", 1);

    __asm__ volatile("sti");

    boot_wait(25);

    keyboard_init();
    boot_status("Keyboard driver", 1);
    boot_wait(25);

    ps2_init();
    boot_status("PS/2 controller", 1);
    boot_wait(25);

    mouse_init();
    boot_status("Mouse driver", 1);
    boot_wait(25);

    filesystem_ok =
        fat32_mount(&filesystem, 0);

    if(filesystem_ok)
    {
        vfs_init(&filesystem);

        boot_status(
            "ATA storage driver",
            1);
        boot_wait(25);

        boot_status(
            "FAT32 filesystem",
            1);
        boot_wait(25);

        boot_status(
            "Virtual filesystem",
            1);
        boot_wait(25);
    }
    else
    {
        filesystem.mounted = 0;

        boot_status(
            "ATA storage driver",
            0);
        boot_wait(25);

        boot_status(
            "FAT32 filesystem",
            0);

        boot_wait(40);

        kernel_panic_disk();
    }

    fm_dir = vfs_get_root();
    fm_index = 0;
    fm_scroll = 0;

    vga_print(
        "Starting turbOS...",
        boot_line + 1,
        3,
        0x0A);

    boot_wait(25);

    if(!accounts_init() || !accounts_boot_login())
        kernel_panic_disk();

    if(gfx_ready())
        desktop_run();

    draw_home();

    second =
        rtc_bcd_to_bin(
            rtc_read(0x00));

    last_second = second;

    for(;;)
    {
        int key;

        while((key = keyboard_getkey()) != KEY_NONE)
        {
            if(key == KEY_CTRL_ALT_S)
            {
                accounts_system_console();
                draw_home();
                continue;
            }

            if(key == KEY_CTRL)
            {
                ctrl_pressed = 1;
                continue;
            }

            if(ctrl_pressed)
            {
                process_global_key(key);
                ctrl_pressed = 0;
                continue;
            }

            if(current_screen == SCREEN_EDITOR)
                handle_editor_key(key);
            else if(current_screen == SCREEN_TERMINAL)
                handle_terminal_key(key);
            else if(current_screen == SCREEN_FILEMANAGER)
                handle_file_manager_key(key);
            else if(current_screen == SCREEN_PONG)
            {
                if(key == 'w' && paddle_y > 1)
                    paddle_y--;

                if(key == 's' && paddle_y < 22)
                    paddle_y++;
            }
        }

        if(current_screen == SCREEN_HOME)
        {
            unsigned int sec =
                rtc_bcd_to_bin(
                    rtc_read(0x00));

            if(sec != last_second)
            {
                last_second = sec;
                home_update_clock();
            }
        }

        if(current_screen == SCREEN_SAVER)
        {
            if(timer_ticks -
               last_saver_tick >= 20)
            {
                last_saver_tick = timer_ticks;
                update_saver();
                draw_saver();
            }
        }

        if(current_screen == SCREEN_PONG)
        {
            if(timer_ticks -
               last_pong_tick >= 5)
            {
                last_pong_tick = timer_ticks;
                update_pong();
            }
        }

        __asm__ volatile("hlt");
    }
}
