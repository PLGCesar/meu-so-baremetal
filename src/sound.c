#include "../include/sound.h"
#include "../include/task.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void sound_play(uint32_t freq) {
    if (freq == 0) return;
    uint32_t div = 1193180 / freq;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void sound_stop(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

// PAUSA SEM GASTAR CPU (HLT ATE O TIMER PIT CONTINUAR)
static void delay_ticks(uint64_t ticks) {
    uint64_t start = get_system_ticks();
    while (get_system_ticks() - start < ticks) {
        asm volatile ("hlt");
    }
}

void sound_click(void) {
    sound_play(1200);
    delay_ticks(1); // 10ms
    sound_stop();
}

void sound_startup(void) {
    sound_play(523);  delay_ticks(8);
    sound_play(659);  delay_ticks(8);
    sound_play(784);  delay_ticks(8);
    sound_play(1046); delay_ticks(15);
    sound_stop();
}
