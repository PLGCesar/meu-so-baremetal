#include "app_serial.h"
#include "serial.h"

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
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
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

void app_serial_send_custom(const char *msg) {
    serial_send_custom(msg);
}

void app_serial_process_host_command(const char *cmd) {
    while (*cmd == ' ') cmd++;

    if (str_starts_with(cmd, "cat ")) {
        const char *filename = cmd + 4;
        while (*filename == ' ') filename++;

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
    } else if (str_starts_with(cmd, "send ")) {
        const char *msg = cmd + 5;
        app_serial_send_custom(msg);
    } else {
        serial_write("[CAPIVARA OS SERIAL]: Comando '");
        serial_write(cmd);
        serial_write("' invalido.\r\nUso: cat <arquivo> ou send <mensagem>\r\n");
    }
}
