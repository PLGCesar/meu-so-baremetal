#include "../include/util.h"
#include "../include/memory.h"

void kmemset(void* dest, uint8_t val, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    if (((uintptr_t)d % 8 == 0) && count >= 8) {
        uint64_t val64 = (uint64_t)val * 0x0101010101010101ULL;
        while (count >= 8) {
            *(uint64_t*)d = val64;
            d += 8;
            count -= 8;
        }
    }
    while (count--) *d++ = val;
}

void kmemcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (((uintptr_t)d % 8 == 0) && ((uintptr_t)s % 8 == 0)) {
        while (count >= 8) {
            *(uint64_t*)d = *(const uint64_t*)s;
            d += 8;
            s += 8;
            count -= 8;
        }
    }
    while (count--) *d++ = *s++;
}

int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int kstrncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void kstrcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

size_t kstrlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

// IMPLEMENTAÇÃO DE ESTRUTURA DE DADOS DINÂMICA DE 64-BITS
kvector_t* kvector_create(size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 4;
    kvector_t* vec = (kvector_t*)kmalloc(sizeof(kvector_t));
    if (!vec) return NULL;
    vec->data = (uint64_t*)kmalloc(initial_cap * sizeof(uint64_t));
    vec->capacity = initial_cap;
    vec->size = 0;
    return vec;
}

void kvector_push(kvector_t* vec, uint64_t val) {
    if (!vec) return;
    if (vec->size >= vec->capacity) {
        size_t new_cap = vec->capacity * 2;
        vec->data = (uint64_t*)krealloc(vec->data, new_cap * sizeof(uint64_t));
        vec->capacity = new_cap;
    }
    vec->data[vec->size++] = val;
}

uint64_t kvector_get(kvector_t* vec, size_t index) {
    if (!vec || index >= vec->size) return 0;
    return vec->data[index];
}

void kvector_free(kvector_t* vec) {
    if (!vec) return;
    if (vec->data) kfree(vec->data);
    kfree(vec);
}
