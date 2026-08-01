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
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000080));
    low |= 1;
    asm volatile ("wrmsr" : : "c"(0xC0000080), "a"(low), "d"(high));

    // Correção: STAR MSR -> Kernel CS=0x08, User CS32=0x18 (A Sysret irá usar offset +16 = 0x28)
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x18 << 48);
    wrmsr(0xC0000081, star);

    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    wrmsr(0xC0000084, 0x200);

    serial_write("[SYSCALL] Instrucoes SYSCALL/SYSRET de 64-bits Habilitadas!\n");
}

uint64_t sys_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (sys_num) {
        case 1:
            gfx_put_pixel((int)arg1, (int)arg2, (uint32_t)arg3);
            return 0;
        case 2:
            return (uint64_t)last_key_pressed;
        case 3:
            sound_play((uint32_t)arg1);
            return 0;
        case 4:
            sound_stop();
            return 0;
        default:
            return (uint64_t)-1;
    }
}
