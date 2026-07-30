#include "../include/sound.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Toca uma frequência específica no PC Speaker (Hz)
void sound_play(uint32_t freq) {
    if (freq == 0) return;
    uint32_t div = 1193180 / freq;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3); // Liga o alto-falante
    }
}

// Desliga o som do alto-falante
void sound_stop(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

// Efeito sonoro de clique rápido para o mouse
void sound_click(void) {
    sound_play(1200);
    for (volatile int i = 0; i < 40000; i++); // Pequena pausa
    sound_stop();
}

// Vinheta de inicialização do SO (Startup Chime Retro C-E-G-C)
void sound_startup(void) {
    sound_play(523);  // Dó (C5)
    for (volatile int i = 0; i < 120000; i++);
    sound_play(659);  // Mi (E5)
    for (volatile int i = 0; i < 120000; i++);
    sound_play(784);  // Sol (G5)
    for (volatile int i = 0; i < 120000; i++);
    sound_play(1046); // Dó Agudo (C6)
    for (volatile int i = 0; i < 250000; i++);
    sound_stop();
}
