#include "../include/syscall.h"
#include "../include/serial.h"
#include "../include/gfx.h"
#include "../include/idt.h"
#include "../include/sound.h"

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    asm volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

extern void syscall_entry(void);

void syscall_init(void) {
    // 1. Ativa o Bit SCE (Syscall Enable) no MSR EFER (0xC0000080)
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000080));
    low |= 1;
    asm volatile ("wrmsr" : : "c"(0xC0000080), "a"(low), "d"(high));

    // 2. Configura STAR MSR (0xC0000081) - Kernel CS=0x08, User CS=0x1B
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x1B << 48);
    wrmsr(0xC0000081, star);

    // 3. Configura LSTAR MSR (0xC0000082) - Ponto de Entrada da Syscall em Assembly
    wrmsr(0xC0000082, (uint64_t)syscall_entry);

    // 4. Configura SFMASK MSR (0xC0000084) - Desativa Interrupções durante a Syscall
    wrmsr(0xC0000084, 0x200);

    serial_write("[SYSCALL] Instrucoes SYSCALL/SYSRET de 64-bits Habilitadas!\n");
}

uint64_t sys_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (sys_num) {
        case 1: // Desenha Pixel (x, y, color)
            gfx_put_pixel((int)arg1, (int)arg2, (uint32_t)arg3);
            return 0;
        case 2: // Lê Tecla
            return (uint64_t)last_key_pressed;
        case 3: // Toca Som (freq)
            sound_play((uint32_t)arg1);
            return 0;
        case 4: // Para Som
            sound_stop();
            return 0;
        default:
            return (uint64_t)-1;
    }
}
