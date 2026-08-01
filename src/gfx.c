#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/klibc.h"

static uint32_t* framebuffer = 0;
static uint32_t* back_buffer = 0;
static size_t width = 1024;
static size_t height = 768;
static size_t pitch = 4096;
static size_t total_pixels_64 = 1024 * 768;

static int dirty_min_x = 1024, dirty_min_y = 768;
static int dirty_max_x = 0, dirty_max_y = 0;
static int dirty_active = 0;

static inline void outw(uint16_t port, uint16_t val) { asm volatile ("outw %w0, %1" : : "a"(val), "Nd"(port)); }
static inline void outl(uint16_t port, uint32_t val) { asm volatile ("outl %k0, %1" : : "a"(val), "Nd"(port)); }
static inline uint32_t inl(uint16_t port) { uint32_t ret; asm volatile ("inl %1, %k0" : "=a"(ret) : "Nd"(port)); return ret; }

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
    ['/'] = {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00},
    ['>'] = {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00},
    ['<'] = {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
    ['+'] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    ['*'] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    ['='] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['#'] = {0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00},
    ['|'] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void gfx_mark_dirty(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int x2 = x + w; int y2 = y + h;
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x2 > (int)width) x2 = (int)width; if (y2 > (int)height) y2 = (int)height;

    if (x < dirty_min_x) dirty_min_x = x; if (y < dirty_min_y) dirty_min_y = y;
    if (x2 > dirty_max_x) dirty_max_x = x2; if (y2 > dirty_max_y) dirty_max_y = y2;
    dirty_active = 1;
}

void gfx_reset_dirty(void) {
    dirty_min_x = (int)width; dirty_min_y = (int)height;
    dirty_max_x = 0; dirty_max_y = 0;
    dirty_active = 0;
}

int gfx_is_dirty(void) { return dirty_active; }

void gfx_init(multiboot_info_t* mbi) {
    (void)mbi;
    outw(0x01CE, 4); outw(0x01CF, 0);
    outw(0x01CE, 1); outw(0x01CF, 1024);
    outw(0x01CE, 2); outw(0x01CF, 768);
    outw(0x01CE, 3); outw(0x01CF, 32);
    outw(0x01CE, 4); outw(0x01CF, 0x01 | 0x40);

    width = 1024; height = 768; pitch = 4096;
    total_pixels_64 = width * height;

    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read_config(0, slot, 0, 0);
        if ((id & 0xFFFF) == 0x1234) {
            uint32_t bar0 = pci_read_config(0, slot, 0, 0x10);
            framebuffer = (uint32_t*)(uintptr_t)(bar0 & 0xFFFFFFF0);
            break;
        }
    }
    if (!framebuffer) framebuffer = (uint32_t*)0xFD000000;

    back_buffer = (uint32_t*)kmalloc(total_pixels_64 * sizeof(uint32_t));
    gfx_reset_dirty();
}

uint32_t gfx_get_width(void) { return (uint32_t)width; }
uint32_t gfx_get_height(void) { return (uint32_t)height; }

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || (size_t)x >= width || y < 0 || (size_t)y >= height) return;
    back_buffer[(size_t)y * width + (size_t)x] = color;
    gfx_mark_dirty(x, y, 1, 1);
}

void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (x < 0 || (size_t)x >= width || y < 0 || (size_t)y >= height) return;
    if (alpha == 0) return;

    size_t offset = (size_t)y * width + (size_t)x;
    if (alpha == 255) { 
        back_buffer[offset] = color; 
    } else {
        uint32_t bg = back_buffer[offset];
        uint32_t r = (((color >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) / 255;
        uint32_t g = (((color >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) / 255;
        uint32_t b = ((color & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) / 255;
        back_buffer[offset] = (r << 16) | (g << 8) | b;
    }
    gfx_mark_dirty(x, y, 1, 1);
}

void gfx_swap_buffers(void) {
    if (!framebuffer || !back_buffer || !dirty_active) return;

    int bw = dirty_max_x - dirty_min_x;
    int bh = dirty_max_y - dirty_min_y;

    if (bw <= 0 || bh <= 0) {
        gfx_reset_dirty();
        return;
    }

    if (bw == (int)width && bh == (int)height) {
        fast_memcpy(framebuffer, back_buffer, total_pixels_64 * sizeof(uint32_t));
    } else {
        size_t line_bytes = (size_t)bw * sizeof(uint32_t);
        for (int y = dirty_min_y; y < dirty_max_y; y++) {
            uint32_t* src = back_buffer + ((size_t)y * width + (size_t)dirty_min_x);
            uint32_t* dst = framebuffer + ((size_t)y * width + (size_t)dirty_min_x);
            fast_memcpy(dst, src, line_bytes);
        }
    }

    gfx_reset_dirty();
}

void gfx_clear(uint32_t color) {
    uint64_t c64 = ((uint64_t)color << 32) | color;
    uint64_t* b64 = (uint64_t*)back_buffer;
    size_t limit = total_pixels_64 / 2;
    for (size_t i = 0; i < limit; i++) {
        b64[i] = c64;
    }
    gfx_mark_dirty(0, 0, (int)width, (int)height);
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x >= (int)width || y >= (int)height || x + w <= 0 || y + h <= 0) return;
    
    int start_x = x < 0 ? 0 : x; int start_y = y < 0 ? 0 : y;
    int end_x = x + w > (int)width ? (int)width : x + w;
    int end_y = y + h > (int)height ? (int)height : y + h;

    for (int i = start_y; i < end_y; i++) {
        uint32_t* row_ptr = back_buffer + (i * width);
        for (int j = start_x; j < end_x; j++) {
            row_ptr[j] = color;
        }
    }
    gfx_mark_dirty(start_x, start_y, end_x - start_x, end_y - start_y);
}

void gfx_draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            gfx_put_pixel_alpha(j, i, color, alpha);
        }
    }
}

void gfx_draw_char(char c, int x, int y, uint32_t color) {
    unsigned char uc = (unsigned char)c;
    if (uc >= 'a' && uc <= 'z') uc -= 32;
    const uint8_t* glyph = font8x8_basic[uc];
    uint32_t w = gfx_get_width();
    uint32_t h = gfx_get_height();

    if (x < 0 || x + 8 > (int)w || y < 0 || y + 8 > (int)h) {
        for (int cy = 0; cy < 8; cy++) {
            uint8_t row = glyph[cy];
            if (!row) continue;
            for (int cx = 0; cx < 8; cx++) {
                if (row & (1 << (7 - cx))) gfx_put_pixel(x + cx, y + cy, color);
            }
        }
        return;
    }

    for (int cy = 0; cy < 8; cy++) {
        uint8_t row = glyph[cy];
        if (!row) continue;
        uint32_t* dst_row = (uint32_t*)((uintptr_t)gfx_get_width() * (y + cy) * 4 + (x * 4));
        (void)dst_row;
        for (int cx = 0; cx < 8; cx++) {
            if (row & (1 << (7 - cx))) gfx_put_pixel(x + cx, y + cy, color);
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

void gfx_draw_number_64(uint64_t num, int x, int y, uint32_t color) {
    char buf[32]; int i = 0;
    if (num == 0) { buf[i++] = '0'; }
    else { while (num > 0) { buf[i++] = '0' + (num % 10); num /= 10; } }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char tmp = buf[j]; buf[j] = buf[i - 1 - j]; buf[i - 1 - j] = tmp; }
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
            if (pixel == '*' || pixel == '.') {
                gfx_put_pixel_alpha(x + cx + 3, y + cy + 3, 0x000000, 90);
            }
        }
    }
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

// RESTAURACAO DAS PAISAGENS PROCEDURAIS COMPLETAS (POR DO SOL, GALAXIA, SYNTHWAVE)
void gfx_draw_landscape_sunset(int x, int y, int w, int h) {
    int horizon = y + (h * 6) / 10;
    int sun_cx = x + w / 2;
    int sun_cy = horizon - (h / 7);

    for (int py = y; py < y + h; py++) {
        int rel_y = py - y;
        for (int px = x; px < x + w; px++) {
            int rel_x = px - x;
            uint32_t color = 0;

            if (py < horizon) {
                int t = (rel_y * 255) / (horizon - y);
                if (t > 255) t = 255;
                uint32_t r, g, b;
                if (t < 128) {
                    int t1 = t * 2;
                    r = 0x1A + ((0xE6 - 0x1A) * t1) / 255;
                    g = 0x05 + ((0x39 - 0x05) * t1) / 255;
                    b = 0x2E + ((0x46 - 0x2E) * t1) / 255;
                } else {
                    int t2 = (t - 128) * 2;
                    r = 0xE6 + ((0xFF - 0xE6) * t2) / 255;
                    g = 0x39 + ((0xB7 - 0x39) * t2) / 255;
                    b = 0x46 + ((0x03 - 0x46) * t2) / 255;
                }
                color = (r << 16) | (g << 8) | b;

                int dx = px - sun_cx; int dy = py - sun_cy;
                int dist2 = dx*dx + dy*dy;
                if (dist2 < 1200) color = 0xFFF1E6;
                else if (dist2 < 2800) color = 0xFFD166;

                int m2 = ((rel_x * 9) % 40) + ((rel_x * 5) % 25);
                if (py > horizon - 35 - m2) color = 0x3A0CA3;

                int m1 = ((rel_x * 13) % 30) + ((rel_x * 19) % 20);
                if (py > horizon - 15 - m1) color = 0x10002B;
            } else {
                int dx = px - sun_cx; if (dx < 0) dx = -dx;
                int ripple = (rel_x * 23 + rel_y * 11) % 17;
                if (dx < 80 && ripple < 9) color = 0xFFD166;
                else if (dx < 160 && ripple < 5) color = 0xF72585;
                else color = 0x050517;
            }
            back_buffer[(size_t)py * width + (size_t)px] = color;
        }
    }
    gfx_mark_dirty(x, y, w, h);
}

void gfx_draw_landscape_cosmos(int x, int y, int w, int h) {
    int cx = x + w / 3, cy = y + h / 2;
    int p_cx = x + (w * 3) / 4, p_cy = y + h / 3;

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            uint32_t color = 0x03030D;

            int hash = (px * 73 + py * 137) % 197;
            if (hash == 7) color = 0xFFFFFF;
            else if (hash == 19) color = 0x4CC9F0;
            else if (hash == 42) color = 0xF72585;

            int dx = px - cx, dy = py - cy;
            int dist2 = dx*dx + dy*dy;
            if (dist2 < 16000) {
                int intensity = (16000 - dist2) * 180 / 16000;
                color += ((intensity) << 16) | (intensity / 4 << 8) | (intensity);
            }

            int pdx = px - p_cx, pdy = py - p_cy;
            int pdist2 = pdx*pdx + pdy*pdy;
            if (pdist2 < 1600) {
                int stripe = (py % 12) < 6 ? 0x4361EE : 0x3F37C9;
                int shadow = (pdx > 10) ? 2 : 1;
                uint32_t r = ((stripe >> 16) & 0xFF) / shadow;
                uint32_t g = ((stripe >> 8) & 0xFF) / shadow;
                uint32_t b = (stripe & 0xFF) / shadow;
                color = (r << 16) | (g << 8) | b;
            }

            int ring_eq = pdx*2 + pdy*6;
            if (ring_eq > -60 && ring_eq < 60 && pdist2 < 4500 && pdist2 > 1800) {
                color = 0x7209B7;
            }
            back_buffer[(size_t)py * width + (size_t)px] = color;
        }
    }
    gfx_mark_dirty(x, y, w, h);
}

void gfx_draw_landscape_synthwave(int x, int y, int w, int h) {
    int horizon = y + (h * 55) / 100;
    int sun_cx = x + w / 2;
    int sun_cy = horizon - (h / 6);

    for (int py = y; py < y + h; py++) {
        int rel_y = py - y;
        for (int px = x; px < x + w; px++) {
            uint32_t color = 0;

            if (py < horizon) {
                int t = (rel_y * 255) / (horizon - y);
                uint32_t r = 0x10 + ((0xFF - 0x10) * t) / 255;
                uint32_t g = 0x00;
                uint32_t b = 0x28 + ((0x70 - 0x28) * t) / 255;
                color = (r << 16) | (g << 8) | b;

                int dx = px - sun_cx, dy = py - sun_cy;
                if (dx*dx + dy*dy < 2500) {
                    int slice = (py - sun_cy + 50) / 8;
                    if (py < sun_cy || (slice % 2 == 0)) color = 0xFFB703;
                }
            } else {
                color = 0x050010;
                int ground_y = py - horizon;

                if (ground_y == 3 || ground_y == 10 || ground_y == 22 || ground_y == 40 ||
                    ground_y == 65 || ground_y == 100 || ground_y == 145 || ground_y == 200) {
                    color = 0xF72585;
                }

                int dx = px - sun_cx;
                if (ground_y > 0) {
                    int perspective = (dx * 120) / ground_y;
                    if (perspective % 35 == 0) color = 0x00FFFF;
                }
            }
            back_buffer[(size_t)py * width + (size_t)px] = color;
        }
    }
    gfx_mark_dirty(x, y, w, h);
}
