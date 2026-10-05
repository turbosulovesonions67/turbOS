#ifndef TURBOS_RTC_H
#define TURBOS_RTC_H

unsigned char rtc_bcd_to_bin(unsigned char value);
unsigned char rtc_read(unsigned char reg);

void rtc_get_time(
    unsigned char *hour,
    unsigned char *minute,
    unsigned char *second
);

#endif
