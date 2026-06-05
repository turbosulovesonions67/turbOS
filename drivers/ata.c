#include "ata.h"

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);

static unsigned short inw(unsigned short port)
{
    unsigned short ret;

    __asm__ volatile(
        "inw %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );

    return ret;
}

static void ata_wait_bsy()
{
    while(inb(ATA_PRIMARY_IO + 7) & 0x80);
}

static void ata_wait_drq()
{
    while(!(inb(ATA_PRIMARY_IO + 7) & 0x08));
}

int ata_read_sector(unsigned int lba, unsigned char* buffer)
{
    ata_wait_bsy();

    outb(ATA_PRIMARY_IO + 2, 1);

    outb(ATA_PRIMARY_IO + 3, (unsigned char)lba);
    outb(ATA_PRIMARY_IO + 4, (unsigned char)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (unsigned char)(lba >> 16));

    outb(
        ATA_PRIMARY_IO + 6,
        0xE0 | ((lba >> 24) & 0x0F)
    );

    outb(ATA_PRIMARY_IO + 7, 0x20);

    ata_wait_bsy();
    ata_wait_drq();

    unsigned short* buf16 = (unsigned short*)buffer;

    for(int i = 0; i < 256; i++)
    {
        buf16[i] = inw(ATA_PRIMARY_IO);
    }

    return 1;
}
