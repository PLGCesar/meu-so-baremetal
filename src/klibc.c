#include "../include/klibc.h"
#include "../include/serial.h"

int avx2_supported = 0;

void klibc_init_cpu_features(void) {
    uint32_t eax = 1, ebx = 0, ecx = 0, edx = 0;
    
    // Testa OSXSAVE (Bit 27 do ECX no CPUID 1)
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
    if (ecx & (1 << 27)) {
        // Checa CPUID Leaf 7 (Suporte Estendido)
        eax = 7; ebx = 0; ecx = 0; edx = 0;
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax), "c"(0));
        
        // Bit 5 do EBX indica se o AVX2 está presente fisicamente
        if (ebx & (1 << 5)) {
            avx2_supported = 1;
        }
    }

    if (avx2_supported) {
        serial_write("[HW] CPU Moderna: Extensoes SIMD AVX2 (256-bits) Ativadas!\n");
    } else {
        serial_write("[HW] CPU Legacy: Modo SSE2 (128-bits) Acionado via Fallback.\n");
    }
}

void fast_memcpy(void* dest, const void* src, size_t n) {
    size_t qwords = n >> 3; size_t bytes = n & 7;
    __asm__ volatile (
        "rep movsq\n\t mov %3, %%rcx\n\t rep movsb"
        : "+D" (dest), "+S" (src), "+c" (qwords)
        : "d" (bytes)
        : "memory"
    );
}

void fast_memset(void* dest, uint8_t val, size_t n) {
    uint64_t val64 = (uint64_t)val * 0x0101010101010101ULL;
    size_t qwords = n >> 3; size_t bytes = n & 7;
    __asm__ volatile (
        "rep stosq\n\t mov %3, %%rcx\n\t rep stosb"
        : "+D" (dest), "+c" (qwords)
        : "a" (val64), "d" (bytes)
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
    size_t len = 0; while (str[len]) len++; return len;
}
