#include "../include/bgif.h"
#include "../include/gfx.h"

#define DELTA_THRESHOLD 12

static int last_drawn_frame = -1;

void bgif_reset_delta_cache(void) {
    last_drawn_frame = -1;
}

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

    if (max_w <= 0 || max_h <= 0 || w <= 0 || h <= 0) return 0;

    uint32_t screen_w = gfx_get_width();
    uint32_t screen_h = gfx_get_height();

    if (dest_x >= (int)screen_w || dest_y >= (int)screen_h) return total_frames;

    int draw_w = max_w;
    int draw_h = max_h;
    if (dest_x + draw_w > (int)screen_w) draw_w = (int)screen_w - dest_x;
    if (dest_y + draw_h > (int)screen_h) draw_h = (int)screen_h - dest_y;

    if (draw_w <= 0 || draw_h <= 0) return total_frames;

    uint32_t step_x = ((uint32_t)w << 16) / max_w;
    uint32_t step_y = ((uint32_t)h << 16) / max_h;

    int force_full_redraw = (last_drawn_frame == -1 || current_frame == 0);
    last_drawn_frame = current_frame;

    int dirty_x1 = dest_x + draw_w, dirty_y1 = dest_y + draw_h;
    int dirty_x2 = dest_x, dirty_y2 = dest_y;
    int pixels_changed = 0;

    uint32_t curr_y = 0;

    for (int py = 0; py < draw_h; py++) {
        uint32_t src_y = (curr_y >> 16);
        if (src_y >= (uint32_t)h) src_y = h - 1;
        curr_y += step_y;

        const uint8_t* row = frame_pixels + (src_y * w * 3);
        int screen_y = dest_y + py;

        if (screen_y >= 0 && screen_y < (int)screen_h) {
            uint32_t curr_x = 0;
            for (int px = 0; px < draw_w; px++) {
                uint32_t src_x = (curr_x >> 16);
                if (src_x >= (uint32_t)w) src_x = w - 1;
                curr_x += step_x;

                int screen_x = dest_x + px;
                if (screen_x >= 0 && screen_x < (int)screen_w) {
                    const uint8_t* pixel = row + (src_x * 3);
                    uint32_t new_color = ((uint32_t)pixel[2] << 16) | ((uint32_t)pixel[1] << 8) | pixel[0];

                    if (!force_full_redraw) {
                        uint32_t old_color = gfx_get_pixel(screen_x, screen_y);
                        int dr = (int)((new_color >> 16) & 0xFF) - (int)((old_color >> 16) & 0xFF);
                        int dg = (int)((new_color >> 8) & 0xFF) - (int)((old_color >> 8) & 0xFF);
                        int db = (int)(new_color & 0xFF) - (int)(old_color & 0xFF);
                        if (dr < 0) dr = -dr;
                        if (dg < 0) dg = -dg;
                        if (db < 0) db = -db;

                        if (dr + dg + db <= DELTA_THRESHOLD) {
                            continue;
                        }
                    }

                    gfx_put_pixel(screen_x, screen_y, new_color);
                    pixels_changed++;

                    if (screen_x < dirty_x1) dirty_x1 = screen_x;
                    if (screen_x > dirty_x2) dirty_x2 = screen_x;
                    if (screen_y < dirty_y1) dirty_y1 = screen_y;
                    if (screen_y > dirty_y2) dirty_y2 = screen_y;
                }
            }
        }
    }

    if (pixels_changed > 0 && dirty_x2 >= dirty_x1 && dirty_y2 >= dirty_y1) {
        gfx_mark_dirty(dirty_x1, dirty_y1, dirty_x2 - dirty_x1 + 1, dirty_y2 - dirty_y1 + 1);
    }

    return total_frames;
}
