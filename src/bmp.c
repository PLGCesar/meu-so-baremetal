#include "../include/bmp.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/util.h"
#include "../include/vfs.h"

typedef struct __attribute__((packed)) {
    uint16_t type; uint32_t size; uint16_t res1; uint16_t res2; uint32_t off_bits;
} bmp_file_header_t;

typedef struct __attribute__((packed)) {
    uint32_t size; int32_t width; int32_t height; uint16_t planes; uint16_t bpp;
    uint32_t comp; uint32_t img_size; int32_t x_ppm; int32_t y_ppm; uint32_t clr_used; uint32_t clr_imp;
} bmp_info_header_t;

int bmp_draw(const uint8_t* bmp_data, int dest_x, int dest_y, int max_w, int max_h) {
    if (!bmp_data) return 0;
    bmp_file_header_t* fh = (bmp_file_header_t*)bmp_data;
    if (fh->type != 0x4D42) return 0;
    bmp_info_header_t* ih = (bmp_info_header_t*)(bmp_data + sizeof(bmp_file_header_t));

    int w = ih->width, h = ih->height, bpp = ih->bpp;
    int is_bottom_up = 1; if (h < 0) { h = -h; is_bottom_up = 0; }
    
    const uint8_t* pixels = bmp_data + fh->off_bits;
    int bytes_per_pixel = bpp / 8;
    int row_size = ((w * bpp + 31) / 32) * 4;

    // AUTO-SCALING (NEAREST NEIGHBOR)
    for (int py = 0; py < max_h; py++) {
        int src_y = (py * h) / max_h;
        int real_y = is_bottom_up ? (h - 1 - src_y) : src_y;
        const uint8_t* row = pixels + (real_y * row_size);

        for (int px = 0; px < max_w; px++) {
            int src_x = (px * w) / max_w;
            const uint8_t* pixel = row + (src_x * bytes_per_pixel);
            uint32_t color = (pixel[2] << 16) | (pixel[1] << 8) | pixel[0];
            gfx_put_pixel(dest_x + px, dest_y + py, color);
        }
    }
    return 1;
}

// BOSS 3: GERADOR MATEMATICO DE ARQUIVOS BMP 24-BITS
int bmp_export(const char* filename, uint32_t* canvas, int w, int h) {
    int row_size = ((w * 24 + 31) / 32) * 4;
    int file_size = 54 + (row_size * h);
    uint8_t* bmp_data = kmalloc(file_size);
    kmemset(bmp_data, 0, file_size);

    bmp_file_header_t* fh = (bmp_file_header_t*)bmp_data;
    fh->type = 0x4D42; fh->size = file_size; fh->off_bits = 54;

    bmp_info_header_t* ih = (bmp_info_header_t*)(bmp_data + 14);
    ih->size = 40; ih->width = w; ih->height = h; ih->planes = 1; ih->bpp = 24;

    uint8_t* pixels = bmp_data + 54;
    for (int y = 0; y < h; y++) {
        int src_y = h - 1 - y; // Grava de baixo para cima
        for (int x = 0; x < w; x++) {
            uint32_t color = canvas[src_y * w + x];
            int dst_idx = y * row_size + x * 3;
            pixels[dst_idx] = color & 0xFF;         // B
            pixels[dst_idx+1] = (color >> 8) & 0xFF;  // G
            pixels[dst_idx+2] = (color >> 16) & 0xFF; // R
        }
    }

    int res = vfs_write_file(filename, bmp_data, file_size);
    kfree(bmp_data);
    return res;
}
