#ifndef KLIBC_H
#define KLIBC_H

#include <stddef.h>
#include <stdint.h>

void klibc_init_cpu_features(void);
void fast_memcpy(void* dest, const void* src, size_t n);
void fast_memset(void* dest, uint8_t val, size_t n);

int kstrcmp(const char* s1, const char* s2);
int kstrncmp(const char* s1, const char* s2, size_t n);
void kstrcpy(char* dest, const char* src);
size_t kstrlen(const char* str);

#endif
