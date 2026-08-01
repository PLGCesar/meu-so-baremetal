#include "../include/klibc.h"

// OTIMIZACAO EXTREMA: SSE2 SIMD 128-bits com escrita Non-Temporal direto na VRAM!
void fast_memcpy(void* dest, const void* src, size_t n) {
    // Se o bloco for >= 64 bytes e alinhado a 16-bytes, usa registradores XMM de 128 bits
    if (n >= 64 && ((uintptr_t)dest % 16 == 0) && ((uintptr_t)src % 16 == 0)) {
        size_t blocks = n >> 4; // n / 16
        size_t remainder = n & 15; // n % 16
        
        __asm__ volatile (
            "1:\n\t"
            "movdqu (%1), %%xmm0\n\t"
            "movntdq %%xmm0, (%0)\n\t"
            "add $16, %0\n\t"
            "add $16, %1\n\t"
            "dec %2\n\t"
            "jnz 1b\n\t"
            "sfence"
            : "+r"(dest), "+r"(src), "+r"(blocks)
            :
            : "xmm0", "memory"
        );
        
        if (remainder > 0) {
            uint8_t* d = (uint8_t*)dest;
            const uint8_t* s = (const uint8_t*)src;
            while (remainder--) *d++ = *s++;
        }
        return;
    }

    // Fallback ultra-rapido rep movsq (8 bytes por ciclo)
    size_t qwords = n >> 3;
    size_t bytes = n & 7;
    __asm__ volatile (
        "rep movsq\n\t"
        "mov %3, %%rcx\n\t"
        "rep movsb"
        : "+D" (dest), "+S" (src), "+c" (qwords)
        : "r" (bytes)
        : "memory"
    );
}

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
