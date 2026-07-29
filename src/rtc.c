#include "../include/rtc.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int is_update_in_progress(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}

void rtc_get_time(rtc_time_t* time) {
    // Aguarda o chip CMOS terminar a atualização dos registradores
    while (is_update_in_progress());

    time->second = get_rtc_register(0x00);
    time->minute = get_rtc_register(0x02);
    time->hour   = get_rtc_register(0x04);
    time->day    = get_rtc_register(0x07);
    time->month  = get_rtc_register(0x08);
    time->year   = get_rtc_register(0x09);

    uint8_t registerB = get_rtc_register(0x0B);

    // Converte de BCD para Binário normal se o chip CMOS estiver em BCD
    if (!(registerB & 0x04)) {
        time->second = bcd_to_bin(time->second);
        time->minute = bcd_to_bin(time->minute);
        time->hour   = bcd_to_bin(time->hour);
        time->day    = bcd_to_bin(time->day);
        time->month  = bcd_to_bin(time->month);
        time->year   = bcd_to_bin(time->year);
    }

    // Trata o formato AM/PM para 24 horas se necessário
    if (!(registerB & 0x02) && (time->hour & 0x80)) {
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }

    time->year += 2000;
}

void rtc_get_time_brt(rtc_time_t* time) {
    rtc_get_time(time);

    // Ajuste exato para o Horário de Brasília (BRT: UTC - 3 Horas)
    if (time->hour >= 3) {
        time->hour -= 3;
    } else {
        time->hour = time->hour + 24 - 3;
    }
}
