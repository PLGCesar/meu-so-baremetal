#include "../include/multiboot.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/idt.h"

static char input_buffer[32];
static int input_index = 0;

void render_gui(void) {
    // 1. Fundo
    gfx_clear(COLOR_DARK_SLATE);

    // 2. Barra de Tarefas
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, COLOR_BLUE);
    gfx_draw_string("START", 30, 576, COLOR_NAVY);
    gfx_draw_string("BARE-METAL OS 800X600", 590, 576, COLOR_WHITE);

    // 3. Janela do Sistema
    int win_x = 80, win_y = 60, win_w = 640, win_h = 460;

    gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B); // Sombra
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);        // Corpo
    gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);    // Barra
    gfx_draw_string("SISTEMA OPERACIONAL (GUI + TECLADO + MOUSE)", win_x + 15, win_y + 11, COLOR_WHITE);

    // Botões fechar/minimizar
    gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);
    gfx_draw_rect(win_x + win_w - 45, win_y + 7, 16, 16, COLOR_BLUE);

    // PAINEL DO TECLADO
    gfx_draw_string("DIGITE NO SEU TECLADO:", win_x + 30, win_y + 50, COLOR_GREEN);
    gfx_draw_rect(win_x + 30, win_y + 75, 580, 35, COLOR_NAVY);
    gfx_draw_string(input_buffer, win_x + 40, win_y + 88, COLOR_WHITE);

    // PAINEL DO MOUSE
    gfx_draw_string("COORDENADAS DO MOUSE EM TEMPO REAL:", win_x + 30, win_y + 130, COLOR_BLUE);
    gfx_draw_string("X: ", win_x + 30, win_y + 160, COLOR_WHITE);
    gfx_draw_number(mouse_x, win_x + 50, win_y + 160, COLOR_GREEN);

    gfx_draw_string("Y: ", win_x + 120, win_y + 160, COLOR_WHITE);
    gfx_draw_number(mouse_y, win_x + 140, win_y + 160, COLOR_GREEN);

    if (mouse_left_clicked) {
        gfx_draw_string("CLIQUE ESQUERDO ATIVO!", win_x + 240, win_y + 160, COLOR_RED);
    }

    // ESTATÍSTICAS DA MEMÓRIA
    gfx_draw_rect(win_x + 30, win_y + 210, win_w - 60, 210, COLOR_NAVY);
    gfx_draw_string("STATUS DA MEMORIA HEAP (RAM):", win_x + 50, win_y + 230, COLOR_WHITE);

    gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 270, COLOR_WHITE);
    gfx_draw_number((int)memory_get_total_allocated(), win_x + 220, win_y + 270, COLOR_GREEN);
    gfx_draw_string(" BYTES", win_x + 290, win_y + 270, COLOR_WHITE);

    gfx_draw_string("TOTAL BLOCOS HEAP: ", win_x + 50, win_y + 310, COLOR_WHITE);
    gfx_draw_number((int)memory_get_block_count(), win_x + 220, win_y + 310, COLOR_WHITE);

    // DIBUJA O CURSOR DO MOUSE NA POSIÇÃO ATUAL!
    gfx_draw_cursor(mouse_x, mouse_y);
}

void kernel_main(multiboot_info_t* mbi) {
    gfx_init(mbi);
    memory_init(mbi);
    idt_init(); // Inicializa Interrupções do Teclado e Mouse!

    // Alocações de teste de memória
    void* p1 = kmalloc(256);
    void* p2 = kmalloc(512);
    (void)p1;
    (void)p2;

    input_buffer[0] = '\0';

    // Loop de Renderização Contínua
    while (1) {
        // Trata a entrada do teclado
        if (last_key_pressed != 0) {
            char c = last_key_pressed;
            last_key_pressed = 0;

            if (c == '\b') {
                if (input_index > 0) input_buffer[--input_index] = '\0';
            } else if (c == '\n') {
                input_index = 0;
                input_buffer[0] = '\0';
            } else if (input_index < 30) {
                input_buffer[input_index++] = c;
                input_buffer[input_index] = '\0';
            }
        }

        render_gui();
        asm volatile ("hlt");
    }
}
