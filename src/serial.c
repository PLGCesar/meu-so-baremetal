#include "../include/serial.h"
#include "../include/vfs.h"
#include "../include/klibc.h"
#include "../include/util.h"
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

static char serial_log_buffer[2048] = "[SERIAL LINK BIDIRECIONAL ATIVO]\n";
static size_t serial_log_len = 33;

static void append_to_log(const char* str) {
    size_t l = kstrlen(str);
    if (serial_log_len + l >= sizeof(serial_log_buffer) - 1) {
        serial_log_len = 0;
        serial_log_buffer[0] = '\0';
    }
    kstrcpy(serial_log_buffer + serial_log_len, str);
    serial_log_len += l;
}

const char* serial_get_log(void) {
    return serial_log_buffer;
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

int serial_has_data(void) {
    return inb(PORT + 5) & 0x01;
}

char serial_getc(void) {
    while (!serial_has_data());
    return (char)inb(PORT);
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

void serial_send_custom(const char* msg) {
    if (!msg || msg[0] == '\0') return;

    serial_write("\n[GUEST -> HOST]: ");
    serial_write(msg);
    serial_write("\n");

    append_to_log("\n[GUEST]: ");
    append_to_log(msg);
}

static char rx_line[256];
static size_t rx_idx = 0;

static void process_serial_host_cmd(const char* cmd) {
    append_to_log("\n[HOST]: ");
    append_to_log(cmd);

    serial_write("\n[HOST CMD RECEBIDO]: ");
    serial_write(cmd);
    serial_write("\n");

    if (kstrncmp(cmd, "cat ", 4) == 0) {
        const char* filename = cmd + 4;
        size_t file_size = 0;
        const uint8_t* content = vfs_read(filename, &file_size);

        if (content && file_size > 0) {
            serial_write("\n--- CONTEUDO DO ARQUIVO '");
            serial_write(filename);
            serial_write("' (COMPACTADO/STREAM SERIAL) ---\n");

            for (size_t i = 0; i < file_size; i++) {
                serial_putc((char)content[i]);
            }

            serial_write("\n--- FIM DO ARQUIVO ---\n");
            append_to_log("\n-> [OK] CONTEUDO ENVIADO AO HOST!");
        } else {
            serial_write("[HOST SERIAL ERRO]: Arquivo '");
            serial_write(filename);
            serial_write("' NAO EXISTE NO VFS!\n");

            append_to_log("\n-> [AVISO] ARQUIVO NAO ENCONTRADO!");
        }
    } else if (kstrcmp(cmd, "ls") == 0) {
        char ls_buf[256];
        vfs_list(ls_buf, sizeof(ls_buf));
        serial_write("\n--- ARQUIVOS VFS ---\n");
        serial_write(ls_buf);
        serial_write("\n--------------------\n");
        append_to_log("\n-> LS EXECUTADO");
    } else if (kstrcmp(cmd, "help") == 0) {
        serial_write("\nCOMANDOS HOST SUPORTADOS: cat <file>, ls, help, echo <msg>\n");
        append_to_log("\n-> HELP EXECUTADO");
    } else if (kstrncmp(cmd, "echo ", 5) == 0) {
        serial_write("[ECHO HOST]: ");
        serial_write(cmd + 5);
        serial_write("\n");
    } else {
        serial_write("[CAPIVARAOS SERIAL]: Comando desconhecido. Digite 'help' ou 'cat <arquivo>'.\n");
    }
}

void serial_poll(void) {
    while (serial_has_data()) {
        char c = (char)inb(PORT);
        if (c == '\r') continue;
        if (c == '\n') {
            rx_line[rx_idx] = '\0';
            if (rx_idx > 0) {
                process_serial_host_cmd(rx_line);
                rx_idx = 0;
            }
        } else if (rx_idx < sizeof(rx_line) - 1) {
            rx_line[rx_idx++] = c;
        }
    }
}
