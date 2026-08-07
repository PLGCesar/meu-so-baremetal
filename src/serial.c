#include "serial.h"

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
    outb(COM1_PORT + 3, 0x80); // Ativa DLAB
    outb(COM1_PORT + 0, 0x01); // 115200 baud
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03); // 8N1
    outb(COM1_PORT + 2, 0xC7); // FIFO
    outb(COM1_PORT + 4, 0x0B); // RTS/DSR

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
    // Substituido spinlock while(1) por loop finito com hint de CPU x86 pause
    for (uint32_t timeout = 50000; timeout > 0; timeout--) {
        if (serial_is_transmit_empty()) {
            outb(COM1_PORT, c);
            break;
        }
        __asm__ volatile ("pause");
    }
}

void serial_write(const char *str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0' && i < 4096; i++) {
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
    serial_write("[GUEST SERIAL MSG]: ");
    serial_write(msg);
    serial_write("\r\n");
}

void serial_mark(const char *label) {
    serial_write("[BOOT MARK]: ");
    if (label) {
        serial_write(label);
    } else {
        serial_write("CHECKPOINT");
    }
    serial_write("\r\n");
}

typedef struct {
    const char *name;
    const char *data;
} RamFile;

static const RamFile sys_files[] = {
    {"teste.txt", "Capivara OS: Conteudo do arquivo teste.txt lido via Serial!\r\nStatus: OK\r\n"},
    {"config.sys", "SERIAL_BAUD=115200\r\nPORT=0x3F8\r\n"},
    {0, 0}
};

static int str_cmp(const char *s1, const char *s2) {
    for (size_t i = 0; i < 256; i++) {
        if (s1[i] == '\0' || s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

static int str_starts_with(const char *str, const char *prefix) {
    for (size_t i = 0; prefix[i] != '\0'; i++) {
        if (str[i] != prefix[i]) return 0;
    }
    return 1;
}

static const char* read_file(const char *name) {
    for (int i = 0; sys_files[i].name != 0; i++) {
        if (str_cmp(sys_files[i].name, name) == 0) {
            return sys_files[i].data;
        }
    }
    return 0;
}

void app_serial_process_host_command(const char *cmd) {
    if (!cmd) return;
    
    size_t offset = 0;
    for (; cmd[offset] == ' ' && offset < 256; offset++);

    const char *p = cmd + offset;

    if (str_starts_with(p, "cat ")) {
        const char *filename = p + 4;
        for (; *filename == ' ' && *filename != '\0'; filename++);

        const char *content = read_file(filename);
        if (content) {
            serial_write("=== INICIO DE ");
            serial_write(filename);
            serial_write(" ===\r\n");
            serial_write(content);
            serial_write("=== FIM DE ARQUIVO ===\r\n");
        } else {
            serial_write("[ERRO CAPIVARA OS]: Arquivo '");
            serial_write(filename);
            serial_write("' nao foi encontrado.\r\n");
        }
    } else if (str_starts_with(p, "send ")) {
        const char *msg = p + 5;
        serial_send_custom(msg);
    } else {
        serial_write("[CAPIVARA OS SERIAL]: Comando '");
        serial_write(p);
        serial_write("' invalido.\r\nUso: cat <arquivo> ou send <mensagem>\r\n");
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
