#include "../include/gfx.h"

static uint32_t* framebuffer = 0;
static uint32_t width = 800;
static uint32_t height = 600;
static uint32_t pitch = 3200;

// --- FUNÇÕES DE COMUNICAÇÃO DE HARDWARE (I/O & PCI) ---
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Escaneia os registradores do Barramento PCI da placa-mãe
static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0x0CF8, address);
    return inl(0x0CFC);
}

// Fonte de texto Bitmap 8x8 incorporada
static const uint8_t font8x8_basic[128][8] = {
    ['A'] = {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},
    ['B'] = {0x3E, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3E, 0x00},
    ['C'] = {0x1C, 0x36, 0x60, 0x60, 0x60, 0x36, 0x1C, 0x00},
    ['D'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x1C, 0x36, 0x60, 0x6E, 0x66, 0x36, 0x1E, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['O'] = {0x1C, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1C, 0x00},
    ['P'] = {0x3E, 0x66, 0x66, 0x3E, 0x60, 0x60, 0x60, 0x00},
    ['R'] = {0x3E, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x66, 0x00},
    ['S'] = {0x1C, 0x36, 0x60, 0x1C, 0x06, 0x36, 0x1C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x1C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void gfx_init(multiboot_info_t* mbi) {
    (void)mbi;

    // 1. FORÇA A PLACA DE VÍDEO BGA DO QEMU A MUDAR PARA 800x600 32-BIT DIRETO VIA PORTA I/O
    outw(0x01CE, 4); outw(0x01CF, 0);           // Desativa
    outw(0x01CE, 1); outw(0x01CF, 800);         // Largura: 800
    outw(0x01CE, 2); outw(0x01CF, 600);         // Altura: 600
    outw(0x01CE, 3); outw(0x01CF, 32);          // BPP: 32 bits
    outw(0x01CE, 4); outw(0x01CF, 0x01 | 0x40); // Ativa VBE + Linear Framebuffer!

    width = 800;
    height = 600;
    pitch = 800 * 4;

    // 2. ESCANEIA O BARRAMENTO PCI PARA ENCONTRAR O ENDEREÇO DA PLACA DE VÍDEO DO QEMU
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read_config(0, slot, 0, 0);
        if ((id & 0xFFFF) == 0x1234) { // Vendor ID da GPU Bochs/QEMU
            uint32_t bar0 = pci_read_config(0, slot, 0, 0x10);
            framebuffer = (uint32_t*)(uintptr_t)(bar0 & 0xFFFFFFF0);
            return;
        }
    }

    // Fallback padrão do QEMU caso o escaneamento PCI não responda
    framebuffer = (uint32_t*)0xFD000000;
}

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (!framebuffer) return;
    if (x < 0 || (uint32_t)x >= width || y < 0 || (uint32_t)y >= height) return;
    
    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y * pitch) + (x * 4));
    *pixel = color;
}

void gfx_clear(uint32_t color) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            gfx_put_pixel(x, y, color);
        }
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            gfx_put_pixel(j, i, color);
        }
    }
}

void gfx_draw_char(char c, int x, int y, uint32_t color) {
    const uint8_t* glyph = font8x8_basic[(unsigned char)c];
    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if (glyph[cy] & (1 << (7 - cx))) {
                gfx_put_pixel(x + cx, y + cy, color);
            }
        }
    }
}

void gfx_draw_string(const char* str, int x, int y, uint32_t color) {
    int cur_x = x;
    for (size_t i = 0; str[i] != '\0'; i++) {
        gfx_draw_char(str[i], cur_x, y, color);
        cur_x += 8;
    }
}
