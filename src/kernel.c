#include "../include/multiboot.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"

static char input_buffer[32];
static int input_index = 0;

// Função auxiliar para desenhar números inteiros na tela gráfica
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

    // Inverte a string de dígitos
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    gfx_draw_string(buf, x, y, color);
}

// Função de renderização completa da Interface Gráfica (GUI)
void render_gui(void) {
    // 1. Fundo do Desktop (Dark Mode)
    gfx_clear(COLOR_DARK_SLATE);

    // 2. Barra de Tarefas (Taskbar inferior)
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, COLOR_BLUE);
    gfx_draw_string("START", 30, 576, COLOR_NAVY);
    gfx_draw_string("BARE-METAL OS 800X600", 590, 576, COLOR_WHITE);

    // 3. Janela Principal
    int win_x = 80, win_y = 50, win_w = 640, win_h = 480;

    gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B); // Sombra da Janela
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);        // Corpo da Janela
    gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);    // Barra de Título
    gfx_draw_string("SISTEMA OPERACIONAL (GUI + TECLADO + MOUSE)", win_x + 15, win_y + 11, COLOR_WHITE);

    // Botões da Janela (Fechar / Minimizar)
    gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);
    gfx_draw_rect(win_x + win_w - 45, win_y + 7, 16, 16, COLOR_BLUE);

    // --- PAINEL DO TECLADO ---
    gfx_draw_string("DIGITE NO SEU TECLADO:", win_x + 30, win_y + 45, COLOR_GREEN);
    gfx_draw_rect(win_x + 30, win_y + 68, 580, 35, COLOR_NAVY);
    gfx_draw_string(input_buffer, win_x + 40, win_y + 80, COLOR_WHITE);

    // --- PAINEL DO MOUSE ---
    gfx_draw_string("COORDENADAS DO MOUSE EM TEMPO REAL:", win_x + 30, win_y + 120, COLOR_BLUE);
    gfx_draw_string("X: ", win_x + 30, win_y + 145, COLOR_WHITE);
    gfx_draw_number(mouse_x, win_x + 50, win_y + 145, COLOR_GREEN);

    gfx_draw_string("Y: ", win_x + 120, win_y + 145, COLOR_WHITE);
    gfx_draw_number(mouse_y, win_x + 140, win_y + 145, COLOR_GREEN);

    if (mouse_left_clicked) {
        gfx_draw_string("CLIQUE ESQUERDO ATIVO!", win_x + 240, win_y + 145, COLOR_RED);
    }

    // --- PAINEL DO GERENCIADOR DE MEMÓRIA ---
    gfx_draw_rect(win_x + 30, win_y + 185, win_w - 60, 260, COLOR_NAVY);
    gfx_draw_string("STATUS DA MEMORIA HEAP (RAM):", win_x + 50, win_y + 205, COLOR_WHITE);

    gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 245, COLOR_WHITE);
    gfx_draw_number((int)memory_get_total_allocated(), win_x + 220, win_y + 245, COLOR_GREEN);
    gfx_draw_string(" BYTES", win_x + 290, win_y + 245, COLOR_WHITE);

    gfx_draw_string("RAM LIVRE DISPONIVEL: ", win_x + 50, win_y + 285, COLOR_WHITE);
    gfx_draw_number((int)memory_get_total_free() / 1024, win_x + 250, win_y + 285, COLOR_GREEN);
    gfx_draw_string(" KB", win_x + 330, win_y + 285, COLOR_WHITE);

    gfx_draw_string("TOTAL BLOCOS HEAP: ", win_x + 50, win_y + 325, COLOR_WHITE);
    gfx_draw_number((int)memory_get_block_count(), win_x + 220, win_y + 325, COLOR_WHITE);

    gfx_draw_string("LOG SERIAL (COM1): ATIVO EM 0X3F8", win_x + 50, win_y + 375, COLOR_GREEN);

    // 4. DESENHA O PONTEIRO DO MOUSE (SETA VISUAL NA POSIÇÃO ATUAL)
    gfx_draw_cursor(mouse_x, mouse_y);
}

void kernel_main(multiboot_info_t* mbi) {
    // 1. Inicializa o Log Serial COM1 para depuração
    serial_init();
    serial_write("[LOG SERIAL] ========================================\n");
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL BARE-METAL v0.4\n");
    serial_write("[LOG SERIAL] ========================================\n");

    // 2. Inicializa o Modo Gráfico VBE 800x600x32
    gfx_init(mbi);
    serial_write("[LOG SERIAL] [OK] Driver Grafico VBE 800x600 TrueColor\n");

    // 3. Inicializa o Gerenciador de Memória Heap (RAM)
    memory_init(mbi);
    serial_write("[LOG SERIAL] [OK] Gerenciador de Memoria Heap Avancado\n");

    // 4. Inicializa Interrupções IDT, Teclado PS/2 e Mouse PS/2
    idt_init();
    serial_write("[LOG SERIAL] [OK] Tabela IDT, Teclado PS/2 e Mouse PS/2\n");

    // Alocações de teste no Gerenciador de Memória
    void* p1 = kmalloc(256);
    void* p2 = kmalloc(512);
    serial_write("[LOG SERIAL] [KMALLOC] Alocados 256 bytes e 512 bytes no Heap\n");

    kfree(p1);
    serial_write("[LOG SERIAL] [KFREE] Liberado bloco p1 com sucesso\n");

    void* p3 = kmalloc(128);
    (void)p2;
    (void)p3;
    serial_write("[LOG SERIAL] [KMALLOC] Re-alocado bloco p3 aproveitando memoria livre\n");

    input_buffer[0] = '\0';
    serial_write("[LOG SERIAL] Kernel pronto! Entrando no loop principal de GUI...\n");

    // Loop principal de renderização contínua e tratamento de entrada
    while (1) {
        // Processa entrada do teclado
        if (last_key_pressed != 0) {
            char c = last_key_pressed;
            last_key_pressed = 0;

            if (c == '\b') { // Backspace
                if (input_index > 0) {
                    input_buffer[--input_index] = '\0';
                    serial_write("[TECLADO] Backspace pressionado\n");
                }
            } else if (c == '\n') { // Enter
                input_index = 0;
                input_buffer[0] = '\0';
                serial_write("[TECLADO] Enter pressionado - Buffer Limpo\n");
            } else if (input_index < 30) {
                input_buffer[input_index++] = c;
                input_buffer[input_index] = '\0';
                serial_write("[TECLADO] Tecla digitada: ");
                char str_tmp[2] = {c, '\0'};
                serial_write(str_tmp);
                serial_write("\n");
            }
        }

        // Redesenha a tela do sistema
        render_gui();

        // Interrompe a CPU até a próxima interrupção de hardware (economiza CPU)
        asm volatile ("hlt");
    }
}
