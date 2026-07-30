#include "../include/bmp.h"
#include "../include/gfx.h"

typedef struct __attribute__((packed)) {
    uint16_t type;          // Assinatura "BM" (0x4D42)
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t off_bits;      // Offset de onde os pixels começam
} bmp_file_header_t;

typedef struct __attribute__((packed)) {
    uint32_t size;          // 40
    int32_t  width;         // Largura da imagem
    int32_t  height;        // Altura da imagem
    uint16_t planes;
    uint16_t bpp;           // 24 ou 32 bits
    uint32_t compression;   // 0 = Sem compressão
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t clr_used;
    uint32_t clr_important;
} bmp_info_header_t;

int bmp_draw(const uint8_t* bmp_data, int dest_x, int dest_y, int max_w, int max_h) {
    if (!bmp_data) return 0;

    bmp_file_header_t* file_header = (bmp_file_header_t*)bmp_data;
    if (file_header->type != 0x4D42) return 0; // Não é um arquivo BMP válido!

    bmp_info_header_t* info_header = (bmp_info_header_t*)(bmp_data + sizeof(bmp_file_header_t));

    int w = info_header->width;
    int h = info_header->height;
    int bpp = info_header->bpp;

    int is_bottom_up = 1;
    if (h < 0) {
        h = -h;
        is_bottom_up = 0;
    }

    const uint8_t* pixels = bmp_data + file_header->off_bits;

    int draw_w = (w > max_w) ? max_w : w;
    int draw_h = (h > max_h) ? max_h : h;

    int bytes_per_pixel = bpp / 8;
    int row_size = ((w * bpp + 31) / 32) * 4; // Alinhamento de 4 bytes por linha no BMP

    for (int py = 0; py < draw_h; py++) {
        int src_y = is_bottom_up ? (h - 1 - py) : py;
        const uint8_t* row = pixels + (src_y * row_size);

        for (int px = 0; px < draw_w; px++) {
            const uint8_t* pixel = row + (px * bytes_per_pixel);
            uint8_t b = pixel[0];
            uint8_t g = pixel[1];
            uint8_t r = pixel[2];
            uint32_t color = (r << 16) | (g << 8) | b;

            gfx_put_pixel(dest_x + px, dest_y + py, color);
        }
    }
    return 1;
}
