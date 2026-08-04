#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <stdint.h>

void serial_init(void);
int serial_has_data(void);
char serial_getc(void);
void serial_putc(char a);
void serial_write(const char* str);
void serial_poll(void);

const char* serial_get_log(void);
void serial_send_custom(const char* msg);

#endif
