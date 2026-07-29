#include "../include/multiboot.h"
#include "../include/gfx.h"

void kernel_main(multiboot_info_t* mbi) {
    // Inicializa o Driver Gráfico VBE
    gfx_init(mbi);

    // 1. Fundo do Desktop (Dark Mode / Catppuccin Slate)
    gfx_clear(COLOR_DARK_SLATE);

    // 2. Barra de Tarefas (Taskbar na parte inferior: Y=560)
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, COLOR_BLUE); // Botão "Start"
    gfx_draw_string("START", 30, 576, COLOR_NAVY);

    // Relógio / Status no canto direito da barra
    gfx_draw_string("BARE-METAL OS 800X600", 610, 576, COLOR_WHITE);

    // 3. Desenhar uma Janela Estilizada no centro da tela
    int win_x = 150, win_y = 100, win_w = 500, win_h = 350;

    // Sombra da Janela
    gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B);

    // Corpo da Janela
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);

    // Barra de Título da Janela
    gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);
    gfx_draw_string("MEU SO BARE-METAL - MODOS GRAFICOS VBE", win_x + 15, win_y + 11, COLOR_WHITE);

    // Botões da Janela (Fechar / Minimizar)
    gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);
    gfx_draw_rect(win_x + win_w - 45, win_y + 7, 16, 16, COLOR_BLUE);

    // Conteúdo da Janela
    gfx_draw_string("BEM VINDO AO MODO GRAFICO REAL!", win_x + 30, win_y + 60, COLOR_GREEN);
    gfx_draw_string("RESOLUCAO: 800X600 TRUE COLOR 32-BITS", win_x + 30, win_y + 90, COLOR_WHITE);

    // Caixas de Exemplo de Cores (Paleta)
    gfx_draw_string("PALETA DE CORES:", win_x + 30, win_y + 140, COLOR_WHITE);
    gfx_draw_rect(win_x + 30,  win_y + 160, 40, 40, COLOR_RED);
    gfx_draw_rect(win_x + 80,  win_y + 160, 40, 40, COLOR_GREEN);
    gfx_draw_rect(win_x + 130, win_y + 160, 40, 40, COLOR_BLUE);
    gfx_draw_rect(win_x + 180, win_y + 160, 40, 40, COLOR_WHITE);

    // Loop infinito mantendo a tela gráfica visível
    while (1) {
        asm volatile ("hlt");
    }
}
