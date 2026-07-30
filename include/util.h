#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>
#include <stdint.h>

void kmemset(void* dest, uint8_t val, size_t count);
void kmemcpy(void* dest, const void* src, size_t count);
int kstrcmp(const char* s1, const char* s2);
int kstrncmp(const char* s1, const char* s2, size_t n);
void kstrcpy(char* dest, const char* src);
size_t kstrlen(const char* str);
#endif
