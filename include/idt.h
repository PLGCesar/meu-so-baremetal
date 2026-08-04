#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_init(void);
void keyboard_handler_main(void);
void mouse_handler_main(void);

extern int mouse_x;
extern int mouse_y;
extern int mouse_left_clicked;
extern char last_key_pressed;

#endif
