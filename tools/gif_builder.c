#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define WIDTH 160
#define HEIGHT 120
#define FRAMES 30

typedef struct __attribute__((packed)) {
    char     magic[4];     // "BGIF"
    uint16_t width;        // 160
    uint16_t height;       // 120
    uint16_t frame_count;  // 30
    uint16_t fps;          // 24
    uint32_t frame_size;   // 160 * 120 * 3 = 57600 bytes
} bgif_header_t;

int main(void) {
    FILE* f = fopen("animacao.bgif", "wb");
    if (!f) {
        printf("Erro ao criar animacao.bgif\n");
        return 1;
    }

    bgif_header_t header;
    header.magic[0] = 'B'; header.magic[1] = 'G';
    header.magic[2] = 'I'; header.magic[3] = 'F';
    header.width = WIDTH;
    header.height = HEIGHT;
    header.frame_count = FRAMES;
    header.fps = 24;
    header.frame_size = WIDTH * HEIGHT * 3;

    fwrite(&header, sizeof(bgif_header_t), 1, f);

    uint8_t frame_buffer[WIDTH * HEIGHT * 3];

    for (int frame = 0; frame < FRAMES; frame++) {
        float angle = (frame * 3.14159f * 2.0f) / FRAMES;
        int cx = WIDTH / 2 + (int)(sinf(angle) * 30.0f);
        int cy = HEIGHT / 2 + (int)(cosf(angle) * 20.0f);

        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int dx = x - cx;
                int dy = y - cy;
                int dist2 = dx*dx + dy*dy;

                uint8_t r = 0, g = 0, b = 0;
                if (dist2 < 1200) {
                    int intensity = (1200 - dist2) * 255 / 1200;
                    r = (uint8_t)(intensity);
                    g = (uint8_t)(intensity / 2 + (frame * 8) % 128);
                    b = (uint8_t)(255 - intensity);
                } else {
                    r = (uint8_t)((x * 255) / WIDTH);
                    g = (uint8_t)((y * 255) / HEIGHT);
                    b = (uint8_t)((frame * 255) / FRAMES);
                }

                int idx = (y * WIDTH + x) * 3;
                frame_buffer[idx]     = b;
                frame_buffer[idx + 1] = g;
                frame_buffer[idx + 2] = r;
            }
        }
        fwrite(frame_buffer, 1, WIDTH * HEIGHT * 3, f);
    }

    fclose(f);
    printf("Video animacao.bgif gerado com sucesso! (30 quadros)\n");
    return 0;
}
