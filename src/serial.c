#include "serial.h"
#include "app_serial.h"

#define SERIAL_LOG_SIZE 4096

static char serial_log_buf[SERIAL_LOG_SIZE];
static size_t serial_log_pos = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void log_append_char(char c) {
    if (serial_log_pos < SERIAL_LOG_SIZE - 1) {
        serial_log_buf[serial_log_pos++] = c;
        serial_log_buf[serial_log_pos] = '\0';
    } else {
        for (size_t i = 0; i < SERIAL_LOG_SIZE - 2; i++) {
            serial_log_buf[i] = serial_log_buf[i + 1];
        }
        serial_log_buf[SERIAL_LOG_SIZE - 2] = c;
        serial_log_buf[SERIAL_LOG_SIZE - 1] = '\0';
    }
}

const char* serial_get_log(void) {
    return serial_log_buf;
}

int serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Desabilita interrupcoes UART
    outb(COM1_PORT + 3, 0x80); // Ativa DLAB (divisor de baud rate)
    outb(COM1_PORT + 0, 0x01); // Divisor LSB = 1 (115200 baud)
    outb(COM1_PORT + 1, 0x00); // Divisor MSB = 0
    outb(COM1_PORT + 3, 0x03); // 8 bits, sem paridade, 1 stop bit (8N1)
    outb(COM1_PORT + 2, 0xC7); // Limpa e habilita FIFOs
    outb(COM1_PORT + 4, 0x0B); // Ativa RTS/DSR

    serial_log_buf[0] = '\0';
    serial_log_pos = 0;

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
    uint32_t timeout = 100000;
    while (!serial_is_transmit_empty() && --timeout);

    if (timeout > 0) {
        outb(COM1_PORT, c);
    }
}

void serial_write(const char *str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_write_char('\r');
            log_append_char('\r');
        }
        serial_write_char(str[i]);
        log_append_char(str[i]);
    }
}

void serial_write_string(const char *str) {
    serial_write(str);
}

void serial_send_custom(const char *msg) {
    if (!msg) return;
    serial_write("[GUEST APP USER MSG]: ");
    serial_write(msg);
    serial_write("\r\n");
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
            serial_write("\r\n");
            app_serial_process_host_command(rx_buf);
            rx_idx = 0;
        }
    } else if (c == '\b' || c == 0x7F) {
        if (rx_idx > 0) {
            rx_idx--;
            serial_write("\b \b");
        }
    } else if (rx_idx < RX_BUF_SIZE - 1) {
        rx_buf[rx_idx++] = c;
        char echo[2] = {c, '\0'};
        serial_write(echo);
    }
}
