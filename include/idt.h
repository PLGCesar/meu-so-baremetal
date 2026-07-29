#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Função de inicialização da Tabela IDT, PIC, Teclado e Mouse
void idt_init(void);

// Variáveis globais do Mouse e Teclado expostas para o Kernel e GUI
extern int mouse_x;
extern int mouse_y;
extern int mouse_left_clicked;
extern char last_key_pressed;

#endif
