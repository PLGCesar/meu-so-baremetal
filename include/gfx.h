#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

#define COLOR_DARK_SLATE 0x1E1E2E
#define COLOR_NAVY       0x181825
#define COLOR_WHITE      0xFFFFFF
#define COLOR_BLUE       0x89B4FA
#define COLOR_GREEN      0xA6E3A1
#define COLOR_RED        0xF38BA8
#define COLOR_GRAY       0x313244
#define COLOR_LIGHT_GRAY 0x45475A
#define COLOR_YELLOW     0xF9E2AF
#define COLOR_PURPLE     0xCBA6F7
#define COLOR_ORANGE     0xFAB387

void gfx_init(multiboot_info_t* mbi);
void gfx_put_pixel(int x, int y, uint32_t color);
void gfx_clear(uint32_t color);
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);
void gfx_draw_char(char c, int x, int y, uint32_t color);
void gfx_draw_string(const char* str, int x, int y, uint32_t color);
void gfx_draw_number(int num, int x, int y, uint32_t color);
void gfx_draw_cursor(int x, int y);
void gfx_swap_buffers(void);

void gfx_draw_landscape_sunset(int x, int y, int w, int h);
void gfx_draw_landscape_cosmos(int x, int y, int w, int h);
void gfx_draw_landscape_synthwave(int x, int y, int w, int h);
#endif
