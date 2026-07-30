#include "../include/gfx.h"
#include "../include/memory.h"

static uint32_t* framebuffer = 0;
static uint32_t* back_buffer = 0;
static uint32_t width = 800;
static uint32_t height = 600;
static uint32_t pitch = 3200;

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

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0x0CF8, address);
    return inl(0x0CFC);
}

static const uint8_t font8x8_basic[128][8] = {
    ['A'] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J'] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q'] = {0x3C, 0x66, 0x66, 0x66, 0x6A, 0x6C, 0x36, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00},
    ['S'] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x1C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x0C, 0x1C, 0x3C, 0x6C, 0xFE, 0x0C, 0x0C, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['('] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    [')'] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void gfx_init(multiboot_info_t* mbi) {
    (void)mbi;
    outw(0x01CE, 4); outw(0x01CF, 0);
    outw(0x01CE, 1); outw(0x01CF, 800);
    outw(0x01CE, 2); outw(0x01CF, 600);
    outw(0x01CE, 3); outw(0x01CF, 32);
    outw(0x01CE, 4); outw(0x01CF, 0x01 | 0x40);

    width = 800; height = 600; pitch = 3200;

    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read_config(0, slot, 0, 0);
        if ((id & 0xFFFF) == 0x1234) {
            uint32_t bar0 = pci_read_config(0, slot, 0, 0x10);
            framebuffer = (uint32_t*)(uintptr_t)(bar0 & 0xFFFFFFF0);
            break;
        }
    }
    if (!framebuffer) framebuffer = (uint32_t*)0xFD000000;

    back_buffer = (uint32_t*)kmalloc(width * height * sizeof(uint32_t));
}

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (!back_buffer) return;
    if (x < 0 || (uint32_t)x >= width || y < 0 || (uint32_t)y >= height) return;
    back_buffer[y * width + x] = color;
}

void gfx_swap_buffers(void) {
    if (!framebuffer || !back_buffer) return;
    for (uint32_t y = 0; y < height; y++) {
        uint32_t* src = &back_buffer[y * width];
        uint32_t* dst = (uint32_t*)((uint8_t*)framebuffer + (y * pitch));
        for (uint32_t x = 0; x < width; x++) {
            dst[x] = src[x];
        }
    }
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
    unsigned char uc = (unsigned char)c;
    if (uc >= 'a' && uc <= 'z') uc -= 32;
    const uint8_t* glyph = font8x8_basic[uc];
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

void gfx_draw_number(int num, int x, int y, uint32_t color) {
    char buf[16]; int i = 0;
    if (num == 0) { buf[i++] = '0'; }
    else {
        while (num > 0) { buf[i++] = '0' + (num % 10); num /= 10; }
    }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j]; buf[j] = buf[i - 1 - j]; buf[i - 1 - j] = tmp;
    }
    gfx_draw_string(buf, x, y, color);
}

const char* cursor_sprite[16] = {
    "*           ",
    "**          ",
    "*.*         ",
    "*..*        ",
    "*...*       ",
    "*....*      ",
    "*.....*     ",
    "*......*    ",
    "*.......*   ",
    "*.....****  ",
    "*..**..*    ",
    "*.*  *..*   ",
    "**    *..*  ",
    "*      *..* ",
    "        **  ",
    "            "
};

void gfx_draw_cursor(int x, int y) {
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 12; cx++) {
            char pixel = cursor_sprite[cy][cx];
            if (pixel == '*') {
                gfx_put_pixel(x + cx, y + cy, COLOR_WHITE);
            } else if (pixel == '.') {
                gfx_put_pixel(x + cx, y + cy, COLOR_NAVY);
            }
        }
    }
}

// =======================================================
// ALGORITMOS MATEMÁTICOS DE PAISAGENS PROCEDURAIS
// =======================================================

// 1. PÔR DO SOL NAS MONTANHAS E OCEANO
void gfx_draw_landscape_sunset(int x, int y, int w, int h) {
    int horizon = y + (h * 6) / 10;
    int sun_cx = x + w / 2;
    int sun_cy = horizon - 25;

    for (int py = y; py < y + h; py++) {
        int rel_y = py - y;
        for (int px = x; px < x + w; px++) {
            int rel_x = px - x;
            uint32_t color = 0;

            if (py < horizon) {
                // Céu: Degradê Roxo (0x330044) -> Laranja (0xFF5500)
                int t = (rel_y * 255) / (horizon - y);
                if (t > 255) t = 255;
                uint32_t r = 0x33 + ((0xFF - 0x33) * t) / 255;
                uint32_t g = 0x00 + ((0x55 - 0x00) * t) / 255;
                uint32_t b = 0x44 + ((0x00 - 0x44) * t) / 255;
                color = (r << 16) | (g << 8) | b;

                // Sol Dourado
                int dx = px - sun_cx;
                int dy = py - sun_cy;
                if (dx*dx + dy*dy < 900) {
                    color = 0xFFEEAA;
                }

                // Silhueta de Montanhas
                int m_height = ((rel_x * 7) % 35) + ((rel_x * 13) % 25);
                if (py > horizon - 15 - m_height) {
                    color = 0x110022;
                }
            } else {
                // Oceano com reflexo do Sol
                int dx = px - sun_cx;
                if (dx < 0) dx = -dx;
                int ripple = (rel_x * 17 + rel_y * 9) % 13;
                if (dx < 70 && ripple < 6) {
                    color = 0xFF8800; // Reflexo dourado
                } else {
                    color = 0x0B061A; // Mar escuro
                }
            }
            gfx_put_pixel(px, py, color);
        }
    }
}

// 2. COSMOS, NEBULOSA E PLANETA COM ANÉIS
void gfx_draw_landscape_cosmos(int x, int y, int w, int h) {
    int cx = x + w / 3;
    int cy = y + h / 2;
    int p_cx = x + (w * 3) / 4;
    int p_cy = y + h / 3;

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            uint32_t color = 0x03030D;

            // Estrelas
            int hash = (px * 73 + py * 137) % 101;
            if (hash == 7) color = 0xFFFFFF;
            else if (hash == 19) color = 0x88EEFF;

            // Nebulosa Rosa
            int dx = px - cx;
            int dy = py - cy;
            int dist2 = dx*dx + dy*dy;
            if (dist2 < 10000) {
                int intensity = (10000 - dist2) * 200 / 10000;
                color += ((intensity) << 16) | (intensity / 3);
            }

            // Planeta Azul
            int pdx = px - p_cx;
            int pdy = py - p_cy;
            int pdist2 = pdx*pdx + pdy*pdy;
            if (pdist2 < 900) {
                color = 0x0088DD;
            } else if (pdx*2 + pdy*5 > -40 && pdx*2 + pdy*5 < 40 && pdist2 < 2500) {
                color = 0xAACCFF; // Anel do Planeta
            }

            gfx_put_pixel(px, py, color);
        }
    }
}

// 3. CYBERPUNK SYNTHWAVE & NEON GRID
void gfx_draw_landscape_synthwave(int x, int y, int w, int h) {
    int horizon = y + h / 2;
    int sun_cx = x + w / 2;

    for (int py = y; py < y + h; py++) {
        int rel_y = py - y;
        for (int px = x; px < x + w; px++) {
            uint32_t color = 0;

            if (py < horizon) {
                int t = (rel_y * 255) / (horizon - y);
                color = ((0x10 + ((0xFF - 0x10) * t) / 255) << 16) |
                        ((0x00) << 8) |
                        (0x30 + ((0x7F - 0x30) * t) / 255);

                // Sol Neon
                int dx = px - sun_cx;
                int dy = py - (horizon - 25);
                if (dx*dx + dy*dy < 1200) {
                    if ((py % 6) > 2) {
                        color = 0xFFEE00;
                    }
                }
            } else {
                // Chão de Grade Neon
                color = 0x050010;
                int ground_y = py - horizon;
                if (ground_y == 5 || ground_y == 15 || ground_y == 30 || ground_y == 50 || ground_y == 80 || ground_y == 120) {
                    color = 0xFF007F; // Rosa neon
                }
                int dx = px - sun_cx;
                if (ground_y > 0) {
                    int perspective = (dx * 100) / ground_y;
                    if (perspective % 30 == 0) {
                        color = 0x00FFFF; // Ciano neon
                    }
                }
            }

            gfx_put_pixel(px, py, color);
        }
    }
}
