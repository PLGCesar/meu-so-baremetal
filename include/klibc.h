#ifndef KLIBC_H
#define KLIBC_H

#include <stddef.h>
#include <stdint.h>

// Funcoes Otimizadas em Hardware (Inline Assembly 64-bits)
void fast_memcpy(void* dest, const void* src, size_t n);
void fast_memset(void* dest, uint8_t val, size_t n);

// Strings padroes da LibC
int kstrcmp(const char* s1, const char* s2);
int kstrncmp(const char* s1, const char* s2, size_t n);
void kstrcpy(char* dest, const char* src);
size_t kstrlen(const char* str);

#endif
