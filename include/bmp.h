#ifndef BMP_H
#define BMP_H

#include <stdint.h>

// Desenha uma imagem .bmp (24-bit ou 32-bit) na posição (dest_x, dest_y)
int bmp_draw(const uint8_t* bmp_data, int dest_x, int dest_y, int max_w, int max_h);

#endif
