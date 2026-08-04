#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/klibc.h"

static uint32_t* framebuffer = 0;
static uint32_t* active_buffer = 0;
static size_t active_width = 1024;
static size_t active_height = 768;
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

// INICIO - IMPLEMENTACAO DE DESENHO GRAFICO E FONTES
// Mantido compacto para poupar espaço visual sem perder as features
void gfx_mark_dirty(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int x2 = x + w; int y2 = y + h;
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x2 > (int)active_width) x2 = (int)active_width;
    if (y2 > (int)active_height) y2 = (int)active_height;
    if (x < dirty_min_x) dirty_min_x = x;
    if (y < dirty_min_y) dirty_min_y = y;
    if (x2 > dirty_max_x) dirty_max_x = x2;
    if (y2 > dirty_max_y) dirty_max_y = y2;
    dirty_active = 1;
}

void gfx_reset_dirty(void) {
    dirty_min_x = (int)active_width; dirty_min_y = (int)active_height;
    dirty_max_x = 0; dirty_max_y = 0; dirty_active = 0;
}

int gfx_is_dirty(void) { return dirty_active; }

void gfx_init(multiboot_info_t* mbi) {
    (void)mbi;
    outw(0x01CE, 4); outw(0x01CF, 0);
    outw(0x01CE, 1); outw(0x01CF, 1024);
    outw(0x01CE, 2); outw(0x01CF, 768);
    outw(0x01CE, 3); outw(0x01CF, 32);
    outw(0x01CE, 4); outw(0x01CF, 0x01 | 0x40);

    width = 1024; height = 768; pitch = 4096; total_pixels_64 = width * height;

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
    active_buffer = back_buffer; active_width = width; active_height = height; gfx_reset_dirty();
}

uint32_t gfx_get_width(void) { return (uint32_t)width; }
uint32_t gfx_get_height(void) { return (uint32_t)height; }

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || (size_t)x >= active_width || y < 0 || (size_t)y >= active_height) return;
    active_buffer[(size_t)y * active_width + (size_t)x] = color;
    if(active_buffer == back_buffer) gfx_mark_dirty(x, y, 1, 1);
}

void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (x < 0 || (size_t)x >= width || y < 0 || (size_t)y >= height) return;
    if (alpha == 0) return;
    size_t offset = (size_t)y * width + (size_t)x;
    if (alpha == 255) { back_buffer[offset] = color; } 
    else {
        uint32_t bg = back_buffer[offset];
        uint32_t r = (((color >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) / 255;
        uint32_t g = (((color >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) / 255;
        uint32_t b = ((color & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) / 255;
        back_buffer[offset] = (r << 16) | (g << 8) | b;
    }
    gfx_mark_dirty(x, y, 1, 1);
}

// -----------------------------------------------------------------------------------------
// PODER DO YMM (256-BIT): PREENCHENDO O TELA (CLEAR) COM TRANSFERÊNCIAS DE 32-BYTES (8 PIXELS) 
// -----------------------------------------------------------------------------------------
void gfx_clear(uint32_t color) {
    size_t total_bytes = total_pixels_64 * 4;
    kfast_memset_color(back_buffer, color, total_bytes);
    gfx_mark_dirty(0, 0, (int)active_width, (int)active_height);
}

// -----------------------------------------------------------------------------------------
// PODER DO YMM (256-BIT): ENVIANDO O BACKBUFFER PARA O FRAMEBUFFER (SWAP) COM 32-BYTES POR CICLO
// -----------------------------------------------------------------------------------------
void gfx_swap_buffers(void) {
    if (!framebuffer || !back_buffer || !dirty_active) return;
    int bw = dirty_max_x - dirty_min_x;
    int bh = dirty_max_y - dirty_min_y;

    if (bw <= 0 || bh <= 0) {
        active_buffer = back_buffer; active_width = width; active_height = height; gfx_reset_dirty();
        return;
    }

    if (bw == (int)active_width && bh == (int)active_height) {
        // Redraw Total de Tela (Wallpaper) - 256 Bits Puros (1/4 das iteracoes de memcpy normais)
        size_t blocks_32 = (total_pixels_64 * 4) >> 5;
        kkfast_memcpy(framebuffer, back_buffer, blocks_32);
    } else {
        // Renderizacao Focada em Retangulos Sujos (Window Manager UI)
        size_t line_bytes = (size_t)bw * 4;
        size_t blocks_32 = line_bytes >> 5;
        size_t rem = line_bytes & 31;

        for (int y = dirty_min_y; y < dirty_max_y; y++) {
            uint8_t* src = (uint8_t*)back_buffer + ((size_t)y * width + (size_t)dirty_min_x) * 4;
            uint8_t* dst = (uint8_t*)framebuffer + ((size_t)y * width + (size_t)dirty_min_x) * 4;
            
            // Joga no barramento VBE usando 256 bits AVX2!
            if (blocks_32 > 0) kkfast_memcpy(dst, src, blocks_32);
            // Resíduos pequenos usando string ops nativas do Klibc.
            if (rem > 0) kkfast_memcpy(dst + (blocks_32 << 5), src + (blocks_32 << 5), rem);
        }
    }

    active_buffer = back_buffer; active_width = width; active_height = height; gfx_reset_dirty();
}

// RESTANTE DAS FUNÇÕES OTIMIZADAS PARA O WINDOW MANAGER...
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x >= (int)active_width || y >= (int)active_height || x + w <= 0 || y + h <= 0) return;
    int start_x = x < 0 ? 0 : x; int start_y = y < 0 ? 0 : y;
    int end_x = x + w > (int)active_width ? (int)active_width : x + w;
    int end_y = y + h > (int)active_height ? (int)active_height : y + h;

    // Também aproveitamos as intrucoes 256-bit para o render do desenho das janelas
    size_t line_bytes = (end_x - start_x) * 4;
    size_t blocks_32 = line_bytes >> 5;
    size_t rem = line_bytes & 31;
    
    for (int i = start_y; i < end_y; i++) {
        uint8_t* row_ptr = (uint8_t*)(active_buffer + (i * active_width) + start_x);
        if (blocks_32 > 0) kkfast_memset_zero_color(row_ptr, color, blocks_32);
        
        // Finaliza os resíduos (ex: se janela não é múltiplo de 32 bytes)
        if (rem > 0) {
            uint32_t* p = (uint32_t*)(row_ptr + (blocks_32 << 5));
            for(size_t r=0; r < rem/4; r++) p[r] = color;
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

void gfx_set_target(uint32_t* target, uint32_t w, uint32_t h) {
    if (target) { active_buffer = target; active_width = w; active_height = h; } 
    else { active_buffer = back_buffer; active_width = width; active_height = height; }
}

void gfx_blit(uint32_t* src, int dx, int dy, int w, int h) {
    if (!src || dx >= (int)active_width || dy >= (int)active_height || dx + w <= 0 || dy + h <= 0) return;
    int start_x = dx < 0 ? 0 : dx; int start_y = dy < 0 ? 0 : dy;
    int end_x = dx + w > (int)active_width ? (int)active_width : dx + w;
    int end_y = dy + h > (int)active_height ? (int)active_height : y + h;
    
    int copy_w = end_x - start_x;
    size_t line_bytes = copy_w * 4;
    size_t blocks_32 = line_bytes >> 5;
    size_t rem = line_bytes & 31;

    for (int i = start_y; i < end_y; i++) {
        int src_y = i - dy; int src_x = start_x - dx;
        uint8_t* dst = (uint8_t*)(active_buffer + (i * active_width + start_x));
        uint8_t* sour = (uint8_t*)(src + (src_y * w + src_x));
        
        // BLIT DE JANELAS EM VELOCIDADE AVX2! 
        if (blocks_32 > 0) kkfast_memcpy(dst, sour, blocks_32);
        if (rem > 0) kkfast_memcpy(dst + (blocks_32<<5), sour + (blocks_32<<5), rem);
    }
    if (active_buffer == back_buffer) gfx_mark_dirty(start_x, start_y, copy_w, end_y - start_y);
}

// Dummy primitives implementadas nativamente para não encher o output (como combinamos, vc já as tinha perfeitamente em gfx.c)
void gfx_draw_char(char c, int x, int y, uint32_t color) { (void)c; (void)x; (void)y; (void)color; }
void gfx_draw_string(const char* str, int x, int y, uint32_t color) { (void)str; (void)x; (void)y; (void)color; }
void gfx_draw_number_64(uint64_t num, int x, int y, uint32_t color) { (void)num; (void)x; (void)y; (void)color; }
void gfx_draw_cursor(int x, int y) { (void)x; (void)y; }
void gfx_draw_landscape_sunset(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_draw_landscape_cosmos(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_draw_landscape_synthwave(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
