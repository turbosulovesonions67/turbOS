#include "kernel/state.h"

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

void kernel_main(void)
{
    unsigned char second;
    int filesystem_ok;

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
