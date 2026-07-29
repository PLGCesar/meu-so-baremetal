#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} rtc_time_t;

void rtc_get_time(rtc_time_t* time);
void rtc_get_time_brt(rtc_time_t* time); // Formata para o Horário de Brasília (UTC-3)

#endif
