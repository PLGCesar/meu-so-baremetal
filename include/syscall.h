#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

void syscall_init(void);
uint64_t sys_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif
