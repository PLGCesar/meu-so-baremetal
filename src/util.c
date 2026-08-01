#include "../include/util.h"

void kmemset(void* dest, uint8_t val, size_t count) {
    uint8_t* d = (uint8_t*)dest;

    // Se o endereço estiver alinhado em 8-bytes e o tamanho for >= 8, faz preenchimento de 64-bits
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

    // Cópia ultra-rápida de 64-bits (8 bytes por instrução) se ambos estiverem alinhados
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
