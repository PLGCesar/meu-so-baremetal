#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ICON_SIZE 24

typedef struct __attribute__((packed)) {
    uint16_t type; uint32_t size; uint16_t res1; uint16_t res2; uint32_t off_bits;
} bmp_fh_t;

typedef struct __attribute__((packed)) {
    uint32_t size; int32_t width; int32_t height; uint16_t planes; uint16_t bpp;
    uint32_t comp; uint32_t img_size; int32_t x_ppm; int32_t y_ppm; uint32_t clr_used; uint32_t clr_imp;
} bmp_ih_t;

static void create_default_bmp(const char* filename, int icon_type) {
    FILE* f = fopen(filename, "rb");
    if (f) { fclose(f); return; } // Ja existe, nao sobrescreve

    f = fopen(filename, "wb");
    if (!f) return;

    int row_size = ((ICON_SIZE * 24 + 31) / 32) * 4;
    int pixel_bytes = row_size * ICON_SIZE;
    int file_size = 54 + pixel_bytes;

    bmp_fh_t fh = { 0x4D42, file_size, 0, 0, 54 };
    bmp_ih_t ih = { 40, ICON_SIZE, ICON_SIZE, 1, 24, 0, pixel_bytes, 2835, 2835, 0, 0 };

    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);

    uint8_t* pixels = (uint8_t*)calloc(1, pixel_bytes);

    // Cores base por tipo de app
    uint8_t r = 100, g = 100, b = 255;
    if (icon_type == 1) { r = 30; g = 200; b = 30; }       // Shell
    else if (icon_type == 2) { r = 255; g = 180; b = 0; }  // RAM
    else if (icon_type == 3) { r = 0; g = 150; b = 255; }  // Net
    else if (icon_type == 4) { r = 200; g = 50; b = 255; } // Gallery
    else if (icon_type == 5) { r = 255; g = 80; b = 80; }  // Music
    else if (icon_type == 6) { r = 180; g = 180; b = 180; }// Tasks
    else if (icon_type == 7) { r = 255; g = 255; b = 0; }  // Paint

    for (int y = 0; y < ICON_SIZE; y++) {
        for (int x = 0; x < ICON_SIZE; x++) {
            int idx = y * row_size + x * 3;
            // Moldura externa
            if (x == 0 || x == ICON_SIZE - 1 || y == 0 || y == ICON_SIZE - 1) {
                pixels[idx] = 255; pixels[idx+1] = 255; pixels[idx+2] = 255;
            } else if (x >= 4 && x <= ICON_SIZE - 5 && y >= 4 && y <= ICON_SIZE - 5) {
                // Desenho interno
                pixels[idx] = b; pixels[idx+1] = g; pixels[idx+2] = r;
            } else {
                // Fundo magenta transparente (0xFF00FF)
                pixels[idx] = 255; pixels[idx+1] = 0; pixels[idx+2] = 255;
            }
        }
    }

    fwrite(pixels, 1, pixel_bytes, f);
    free(pixels);
    fclose(f);
}

int main(void) {
    FILE* txt = fopen("bmp.txt", "r");
    if (!txt) {
        printf("Erro: bmp.txt nao encontrado!\n");
        return 1;
    }

    FILE* asm_out = fopen("src/bmp_assets.s", "w");
    FILE* h_out = fopen("include/bmp_assets.h", "w");
    FILE* c_out = fopen("src/bmp_assets.c", "w");

    fprintf(asm_out, ".section .rodata\n");
    fprintf(h_out, "#ifndef BMP_ASSETS_H\n#define BMP_ASSETS_H\n#include <stddef.h>\n#include <stdint.h>\n\n");
    fprintf(h_out, "typedef struct {\n    const char* name;\n    const uint8_t* start;\n    const uint8_t* end;\n} embedded_asset_t;\n\n");
    fprintf(h_out, "extern const embedded_asset_t g_embedded_assets[];\nextern const size_t g_embedded_assets_count;\n\n#endif\n");

    fprintf(c_out, "#include \"../include/bmp_assets.h\"\n\n");

    char line[128];
    char names[64][64];
    char syms[64][64];
    int count = 0;

    while (fgets(line, sizeof(line), txt)) {
        // Clear newline
        char* p = strchr(line, '\r'); if (p) *p = 0;
        p = strchr(line, '\n'); if (p) *p = 0;
        if (strlen(line) == 0) continue;

        strcpy(names[count], line);

        // Sanitize symbol name
        char sym[64];
        int j = 0;
        for (int i = 0; line[i]; i++) {
            if (line[i] >= 'a' && line[i] <= 'z') sym[j++] = line[i];
            else if (line[i] >= 'A' && line[i] <= 'Z') sym[j++] = line[i];
            else if (line[i] >= '0' && line[i] <= '9') sym[j++] = line[i];
            else sym[j++] = '_';
        }
        sym[j] = '\0';
        strcpy(syms[count], sym);

        create_default_bmp(line, count % 8);

        fprintf(asm_out, ".global asset_%s_start; .global asset_%s_end\n", sym, sym);
        fprintf(asm_out, "asset_%s_start: .incbin \"%s\"; asset_%s_end:\n\n", sym, line, sym);

        fprintf(c_out, "extern const uint8_t asset_%s_start[];\n", sym);
        fprintf(c_out, "extern const uint8_t asset_%s_end[];\n", sym);

        count++;
    }
    fclose(txt);
    fclose(asm_out);

    fprintf(c_out, "\nconst embedded_asset_t g_embedded_assets[] = {\n");
    for (int i = 0; i < count; i++) {
        fprintf(c_out, "    { \"%s\", asset_%s_start, asset_%s_end },\n", names[i], syms[i]);
    }
    fprintf(c_out, "};\n\nconst size_t g_embedded_assets_count = %d;\n", count);

    fclose(h_out);
    fclose(c_out);

    printf("[ASSETS] Gerados %d assets de bitmaps com sucesso!\n", count);
    return 0;
}
