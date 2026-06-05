#ifndef ATA_H
#define ATA_H

#define ATA_PRIMARY_IO   0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

int ata_read_sector(unsigned int lba, unsigned char* buffer);

#endif
