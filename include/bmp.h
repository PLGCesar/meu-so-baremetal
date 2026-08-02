#ifndef BMP_H
#define BMP_H

#include <stdint.h>
#include <stddef.h>

int bmp_draw(const uint8_t* bmp_data, int dest_x, int dest_y, int max_w, int max_h);
int bmp_draw_icon(const uint8_t* bmp_data, int dest_x, int dest_y, int max_w, int max_h, uint32_t transparent_key);
int bmp_export(const char* filename, uint32_t* canvas, int w, int h);

#endif
