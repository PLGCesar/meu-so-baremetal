#ifndef BGIF_H
#define BGIF_H

#include <stdint.h>
#include <stddef.h>

typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t frame_count;
    uint16_t fps;
    uint32_t frame_size;
} bgif_header_t;

int bgif_draw_frame(const uint8_t* bgif_data, size_t file_size, int frame_idx, int dest_x, int dest_y, int max_w, int max_h);
void bgif_reset_delta_cache(void);

#endif
