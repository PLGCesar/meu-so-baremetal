void gfx_clear(uint32_t color) {
    size_t total_bytes = total_pixels_64 * 4;
    kfast_memset_color(back_buffer, color, total_bytes);
    gfx_mark_dirty(0, 0, (int)active_width, (int)active_height);
}
