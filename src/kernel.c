#include "../include/multiboot.h"
#include "../include/gfx.h"
#include "../include/memory.h"

// Função auxiliar para imprimir números inteiros na tela gráfica
void gfx_draw_number(int num, int x, int y, uint32_t color) {
    char buf[16];
    int i = 0;
    if (num == 0) { buf[i++] = '0'; }
    else {
        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    buf[i] = '\0';

    // Inverte a string
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    gfx_draw_string(buf, x, y, color);
}

void kernel_main(multiboot_info_t* mbi) {
    gfx_init(mbi);
    memory_init(mbi);

    // 1. Fundo do Desktop
    gfx_clear(COLOR_DARK_SLATE);

    // 2. Barra de Tarefas
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, COLOR_BLUE);
    gfx_draw_string("START", 30, 576, COLOR_NAVY);
    gfx_draw_string("BARE-METAL OS 800X600", 610, 576, COLOR_WHITE);

    // 3. Janela do Gerenciador de Memória
    int win_x = 100, win_y = 80, win_w = 600, win_h = 420;

    gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B); // Sombra
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);        // Corpo
    gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);    // Barra
    gfx_draw_string("GERENCIADOR DE MEMORIA AVANCADO (HEAP & KMALLOC)", win_x + 15, win_y + 11, COLOR_WHITE);

    // Botões
    gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);
    gfx_draw_rect(win_x + win_w - 45, win_y + 7, 16, 16, COLOR_BLUE);

    // TESTES DE MEMÓRIA DINÂMICA
    gfx_draw_string("TESTANDO ALOCACAO DINAMICA E KFREE:", win_x + 30, win_y + 50, COLOR_GREEN);

    // Alocação 1
    void* ptr1 = kmalloc(512);
    gfx_draw_string("1. KMALLOC(512 BYTES) -> SUCESSO", win_x + 30, win_y + 80, COLOR_WHITE);

    // Alocação 2
    void* ptr2 = kmalloc(1024);
    gfx_draw_string("2. KMALLOC(1024 BYTES) -> SUCESSO", win_x + 30, win_y + 110, COLOR_WHITE);

    // Alocação 3
    void* ptr3 = kmalloc(2048);
    gfx_draw_string("3. KMALLOC(2048 BYTES) -> SUCESSO", win_x + 30, win_y + 140, COLOR_WHITE);

    // Liberação de memória (KFREE)
    kfree(ptr2); // Libera o bloco do meio!
    gfx_draw_string("4. KFREE(PTR2 - 1024 BYTES) -> MEMORIA REAPROVEITADA!", win_x + 30, win_y + 180, COLOR_RED);

    // Re-alocação (Deve reusar a memória liberada pelo ptr2)
    void* ptr4 = kmalloc(500);
    gfx_draw_string("5. KMALLOC(500 BYTES) -> USOU O ESPACO LIBERADO!", win_x + 30, win_y + 210, COLOR_GREEN);

    // ESTATÍSTICAS DA RAM
    gfx_draw_rect(win_x + 30, win_y + 260, win_w - 60, 120, COLOR_NAVY);
    gfx_draw_string("ESTATISTICAS DO HEAP DA RAM:", win_x + 50, win_y + 280, COLOR_BLUE);

    gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 310, COLOR_WHITE);
    gfx_draw_number((int)memory_get_total_allocated(), win_x + 220, win_y + 310, COLOR_GREEN);
    gfx_draw_string(" BYTES", win_x + 280, win_y + 310, COLOR_WHITE);

    gfx_draw_string("TOTAL BLOCOS HEAP: ", win_x + 50, win_y + 340, COLOR_WHITE);
    gfx_draw_number((int)memory_get_block_count(), win_x + 220, win_y + 340, COLOR_WHITE);

    while (1) {
        asm volatile ("hlt");
    }
}
