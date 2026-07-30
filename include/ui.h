#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "multiboot.h"

void ui_init(void);
void ui_render(void);
void ui_handle_mouse(void);
void ui_handle_keyboard(void);

#endif
