#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

#define COM1_PORT 0x3F8

int serial_init(void);
int serial_has_data(void);
char serial_read_char(void);
void serial_write_char(char c);

// Simbolos de ligacao com o Kernel (ld)
void serial_write(const char *str);
void serial_write_string(const char *str);
const char* serial_get_log(void);
void serial_send_custom(const char *msg);
void app_serial_process_host_command(const char *cmd);

// Debug e Polling Nao-Bloqueante
void serial_mark(const char *label);
void serial_poll(void);

#endif
