#include "../include/klibc.h"

static int avx_supported = 0;

void klibc_init_cpu_features(void) {
    uint32_t eax = 1, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
    if ((ecx & (1 << 27)) && (ecx & (1 << 28))) {
        avx_supported = 1;
    }
}

// OTIMIZACAO MONSTRUOSA: Usa AVX 256-bits (vmovntdq %ymm) -> 32 BYTES POR CICLO!
void fast_memcpy(void* dest, const void* src, size_t n) {
    if (!dest || !src || n == 0) return;

    if (!avx_supported) klibc_init_cpu_features();

    // SE A CPU SUPORTAR AVX 256-BITS E O BLOCO FOR >= 128 BYTES:
    if (avx_supported && n >= 128 && ((uintptr_t)dest % 32 == 0) && ((uintptr_t)src % 32 == 0)) {
        size_t blocks_32 = n >> 5; // n / 32
        size_t remainder = n & 31; // n % 32
        
        __asm__ volatile (
            "1:\n\t"
            "vmovdqu (%1), %%ymm0\n\t"
            "vmovntdq %%ymm0, (%0)\n\t" // ESCRITA NON-TEMPORAL DE 32 BYTES POR CICLO!
            "add $32, %0\n\t"
            "add $32, %1\n\t"
            "dec %2\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(dest), "+r"(src), "+r"(blocks_32)
            :
            : "ymm0", "memory"
        );

        if (remainder > 0) {
            uint8_t* d = (uint8_t*)dest + (blocks_32 << 5);
            const uint8_t* s = (const uint8_t*)src + (blocks_32 << 5);
            while (remainder--) *d++ = *s++;
        }
        return;
    }

    // FALLBACK 64-BITS REP MOVSQ (8 BYTES POR CICLO)
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
