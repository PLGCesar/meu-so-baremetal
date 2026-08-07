#include "serial.h"
#include "app_serial.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

int serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Desabilita interrupcoes UART
    outb(COM1_PORT + 3, 0x80); // Ativa DLAB (divisor de baud rate)
    outb(COM1_PORT + 0, 0x01); // Divisor LSB = 1 (115200 baud)
    outb(COM1_PORT + 1, 0x00); // Divisor MSB = 0
    outb(COM1_PORT + 3, 0x03); // 8 bits, sem paridade, 1 stop bit (8N1)
    outb(COM1_PORT + 2, 0xC7); // Limpa e habilita FIFOs (14 bytes)
    outb(COM1_PORT + 4, 0x0B); // Ativa RTS/DSR e linhas de controle

    return 0;
}

int serial_has_data(void) {
    return (inb(COM1_PORT + 5) & 0x01) != 0;
}

char serial_read_char(void) {
    if (!serial_has_data()) return '\0';
    return inb(COM1_PORT);
}

static int serial_is_transmit_empty(void) {
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

void serial_write_char(char c) {
    // Timeout para evitar que o Kernel trave no boot caso a UART falhe
    uint32_t timeout = 100000;
    while (!serial_is_transmit_empty() && --timeout);
    
    if (timeout > 0) {
        outb(COM1_PORT, c);
    }
}

void serial_write_string(const char *str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(str[i]);
    }
}

#define RX_BUF_SIZE 256
static char rx_buf[RX_BUF_SIZE];
static size_t rx_idx = 0;

void serial_poll(void) {
    if (!serial_has_data()) return;

    char c = serial_read_char();
    if (c == '\0') return;

    if (c == '\r' || c == '\n') {
        if (rx_idx > 0) {
            rx_buf[rx_idx] = '\0';
            serial_write_string("\r\n");
            app_serial_process_host_command(rx_buf);
            rx_idx = 0;
        }
    } else if (c == '\b' || c == 0x7F) {
        if (rx_idx > 0) {
            rx_idx--;
            serial_write_string("\b \b");
        }
    } else if (rx_idx < RX_BUF_SIZE - 1) {
        rx_buf[rx_idx++] = c;
        char echo[2] = {c, '\0'};
        serial_write_string(echo);
    }
}
