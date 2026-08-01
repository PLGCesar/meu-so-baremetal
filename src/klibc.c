#include "../include/klibc.h"

// OTIMIZACAO: Usa a instrucao da CPU x86_64 "rep movsq" para mover 8 bytes por ciclo!
void fast_memcpy(void* dest, const void* src, size_t n) {
    size_t qwords = n >> 3;  // n / 8
    size_t bytes = n & 7;    // n % 8
    
    __asm__ volatile (
        "rep movsq\n\t"
        "mov %3, %%rcx\n\t"
        "rep movsb"
        : "+D" (dest), "+S" (src), "+c" (qwords)
        : "r" (bytes)
        : "memory"
    );
}

// OTIMIZACAO: Usa "rep stosq" para preencher memoria em velocidade maxima!
void fast_memset(void* dest, uint8_t val, size_t n) {
    uint64_t val64 = (uint64_t)val * 0x0101010101010101ULL;
    size_t qwords = n >> 3;
    size_t bytes = n & 7;

    __asm__ volatile (
        "rep stosq\n\t"
        "mov %3, %%rcx\n\t"
        "rep stosb"
        : "+D" (dest), "+c" (qwords)
        : "a" (val64), "r" (bytes)
        : "memory"
    );
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
