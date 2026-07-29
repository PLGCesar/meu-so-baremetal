#include "../include/serial.h"
#include <stdint.h>

#define PORT 0x3F8 // Porta Serial COM1

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void serial_init(void) {
    outb(PORT + 1, 0x00);    // Desativa interrupções seriais
    outb(PORT + 3, 0x80);    // Habilita DLAB
    outb(PORT + 0, 0x03);    // Baud rate 38400
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x03);    // 8 bits, sem paridade, 1 stop bit
    outb(PORT + 2, 0xC7);    // Ativa FIFO
    outb(PORT + 4, 0x0B);    // Ativa IRQs, RTS/DSR
}

int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

void serial_putc(char a) {
    while (is_transmit_empty() == 0);
    outb(PORT, a);
}

void serial_write(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putc(str[i]);
    }
}
