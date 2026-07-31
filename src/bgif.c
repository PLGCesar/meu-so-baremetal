#include "../include/bgif.h"
#include "../include/gfx.h"

int bgif_draw_frame(const uint8_t* bgif_data, size_t file_size, int frame_idx, int dest_x, int dest_y, int max_w, int max_h) {
    if (!bgif_data || file_size < sizeof(bgif_header_t)) return 0;

    bgif_header_t* header = (bgif_header_t*)bgif_data;
    if (header->magic[0] != 'B' || header->magic[1] != 'G' ||
        header->magic[2] != 'I' || header->magic[3] != 'F') {
        return 0;
    }

    int total_frames = header->frame_count;
    if (total_frames <= 0) return 0;

    int current_frame = frame_idx % total_frames;
    uint32_t frame_offset = sizeof(bgif_header_t) + (current_frame * header->frame_size);

    if (frame_offset + header->frame_size > file_size) return 0;

    const uint8_t* frame_pixels = bgif_data + frame_offset;
    int w = header->width;
    int h = header->height;

    for (int py = 0; py < max_h; py++) {
        int src_y = (py * h) / max_h;
        const uint8_t* row = frame_pixels + (src_y * w * 3);

        for (int px = 0; px < max_w; px++) {
            int src_x = (px * w) / max_w;
            const uint8_t* pixel = row + (src_x * 3);
            uint32_t color = (pixel[2] << 16) | (pixel[1] << 8) | pixel[0];
            gfx_put_pixel(dest_x + px, dest_y + py, color);
        }
    }
    return total_frames;
}
