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

// VETOR DINÂMICO DE 64-BITS (DYNAMIC ARRAY LIST)
typedef struct {
    uint64_t* data;
    size_t capacity;
    size_t size;
} kvector_t;

kvector_t* kvector_create(size_t initial_cap);
void kvector_push(kvector_t* vec, uint64_t val);
uint64_t kvector_get(kvector_t* vec, size_t index);
void kvector_free(kvector_t* vec);

#endif
