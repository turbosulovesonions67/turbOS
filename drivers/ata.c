#include "ata.h"

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

static unsigned char inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile("inb %1,%0" : "=a"(value) : "Nd"(port));

    return value;
}

static unsigned short inw(unsigned short port)
{
    unsigned short value;

    __asm__ volatile("inw %1,%0" : "=a"(value) : "Nd"(port));

    return value;
}

static void outw(unsigned short port, unsigned short value)
{
    __asm__ volatile("outw %0,%1" : : "a"(value), "Nd"(port));
}

static int ata_wait_bsy(void)
{
    int timeout = 100000;

    while((inb(ATA_PRIMARY_IO + 7) & 0x80) && timeout--)
    {
    }

    return timeout > 0;
}

static int ata_wait_drq(void)
{
    int timeout = 100000;

    while(!(inb(ATA_PRIMARY_IO + 7) & 0x08) && timeout--)
    {
        if(inb(ATA_PRIMARY_IO + 7) & 0x01)
            return 0;
    }

    return timeout > 0;
}

int ata_read_sector(unsigned int lba, unsigned char *buffer)
{
    unsigned short *buf = (unsigned short *)buffer;

    if(!ata_wait_bsy())
        return 0;

    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, lba & 0xFF);
    outb(ATA_PRIMARY_IO + 4, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_IO + 5, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + 7, 0x20);

    if(!ata_wait_bsy() || !ata_wait_drq())
        return 0;

    for(int i = 0; i < 256; i++)
        buf[i] = inw(ATA_PRIMARY_IO);

    return 1;
}

int ata_write_sector(unsigned int lba, const unsigned char *buffer)
{
    const unsigned short *buf = (const unsigned short *)buffer;

    if(!ata_wait_bsy())
        return 0;

    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, lba & 0xFF);
    outb(ATA_PRIMARY_IO + 4, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_IO + 5, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + 7, 0x30);

    if(!ata_wait_bsy() || !ata_wait_drq())
        return 0;

    for(int i = 0; i < 256; i++)
        outw(ATA_PRIMARY_IO, buf[i]);

    outb(ATA_PRIMARY_IO + 7, 0xE7);

    return ata_wait_bsy();
}
