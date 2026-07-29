#include "../include/multiboot.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/rtc.h"

// ESTADO DA JANELA
static int win_x = 100, win_y = 60, win_w = 600, win_h = 450;
static int win_open = 1;
static int is_dragging = 0;
static int drag_off_x = 0, drag_off_y = 0;

// ESTADO DO MENU START
static int start_menu_open = 0;
static int active_app = 0; // 0 = Shell, 1 = Memoria, 2 = IDT

// ESTADO DO SHELL (CLI)
static char input_buffer[32];
static int input_index = 0;
static char shell_output[64] = "BEM VINDO AO TERMINAL BARE-METAL! DIGITE 'HELP'.";

static int prev_mouse_left = 0;

int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// FORMATADOR DE HORA HH:MM:SS BRT
void format_time_string(char* buf, uint8_t h, uint8_t m, uint8_t s) {
    buf[0] = '0' + (h / 10);
    buf[1] = '0' + (h % 10);
    buf[2] = ':';
    buf[3] = '0' + (m / 10);
    buf[4] = '0' + (m % 10);
    buf[5] = ':';
    buf[6] = '0' + (s / 10);
    buf[7] = '0' + (s % 10);
    buf[8] = ' ';
    buf[9] = 'B';
    buf[10] = 'R';
    buf[11] = 'T';
    buf[12] = '\0';
}

void process_shell_command(void) {
    if (kstrcmp(input_buffer, "help") == 0) {
        for (int i = 0; i < 64; i++) shell_output[i] = '\0';
        const char* msg = "COMANDOS: HELP, CLEAR, RAM, PANIC";
        for (int i = 0; msg[i] != '\0'; i++) shell_output[i] = msg[i];
    } else if (kstrcmp(input_buffer, "clear") == 0) {
        shell_output[0] = '\0';
    } else if (kstrcmp(input_buffer, "ram") == 0) {
        for (int i = 0; i < 64; i++) shell_output[i] = '\0';
        const char* msg = "RAM DISPONIVEL NO HEAP: 16 MB";
        for (int i = 0; msg[i] != '\0'; i++) shell_output[i] = msg[i];
    } else if (kstrcmp(input_buffer, "panic") == 0) {
        serial_write("[SHELL] TESTANDO KERNEL PANIC!\n");
        int a = 10, b = 0, c = a / b;
        (void)c;
    } else if (input_buffer[0] != '\0') {
        for (int i = 0; i < 64; i++) shell_output[i] = '\0';
        const char* msg = "COMANDO DESCONHECIDO! DIGITE 'HELP'.";
        for (int i = 0; msg[i] != '\0'; i++) shell_output[i] = msg[i];
    }
    input_index = 0;
    input_buffer[0] = '\0';
}

void handle_mouse_events(void) {
    int click = mouse_left_clicked;
    int just_pressed = (click && !prev_mouse_left);
    prev_mouse_left = click;

    // 1. Clique no Botão START
    if (just_pressed && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= 565 && mouse_y <= 595) {
        start_menu_open = !start_menu_open;
        return;
    }

    // 2. Clique nas opções do Menu Start
    if (start_menu_open && just_pressed && mouse_x >= 10 && mouse_x <= 200 && mouse_y >= 430 && mouse_y <= 560) {
        if (mouse_y >= 435 && mouse_y < 470) { active_app = 0; win_open = 1; }
        else if (mouse_y >= 470 && mouse_y < 510) { active_app = 1; win_open = 1; }
        else if (mouse_y >= 510 && mouse_y <= 555) { active_app = 2; win_open = 1; }
        start_menu_open = 0;
        return;
    }

    if (start_menu_open && just_pressed) start_menu_open = 0;
    if (!win_open) return;

    // 3. Clique no Botão Fechar [X]
    if (just_pressed && mouse_x >= (win_x + win_w - 25) && mouse_x <= (win_x + win_w - 9) &&
        mouse_y >= (win_y + 7) && mouse_y <= (win_y + 23)) {
        win_open = 0;
        is_dragging = 0;
        return;
    }

    // 4. Arrasto da Janela (Drag & Drop)
    if (just_pressed && mouse_x >= win_x && mouse_x <= (win_x + win_w - 50) &&
        mouse_y >= win_y && mouse_y <= (win_y + 30)) {
        is_dragging = 1;
        drag_off_x = mouse_x - win_x;
        drag_off_y = mouse_y - win_y;
    }

    if (click && is_dragging) {
        win_x = mouse_x - drag_off_x;
        win_y = mouse_y - drag_off_y;
        if (win_x < 0) win_x = 0;
        if (win_x + win_w > 800) win_x = 800 - win_w;
        if (win_y < 0) win_y = 0;
        if (win_y + win_h > 555) win_y = 555 - win_h;
    }

    if (!click) is_dragging = 0;
}

void render_gui(void) {
    gfx_clear(COLOR_DARK_SLATE);

    // BARRA DE TAREFAS
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, start_menu_open ? COLOR_GREEN : COLOR_BLUE);
    gfx_draw_string("START", 30, 576, COLOR_NAVY);

    // RELÓGIO EM TEMPO REAL NO FUSO HORÁRIO DO BRASIL (BRT / UTC-3)
    rtc_time_t clock_brt;
    rtc_get_time_brt(&clock_brt);
    char time_str[16];
    format_time_string(time_str, clock_brt.hour, clock_brt.minute, clock_brt.second);
    
    // Exibe o Relógio no canto inferior direito da barra de tarefas
    gfx_draw_string(time_str, 680, 576, COLOR_WHITE);

    // JANELA PRINCIPAL
    if (win_open) {
        gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B);
        gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);
        gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);

        if (active_app == 0) gfx_draw_string("JANELA: TERMINAL SHELL (CLI)", win_x + 15, win_y + 11, COLOR_WHITE);
        else if (active_app == 1) gfx_draw_string("JANELA: GERENCIADOR DE MEMORIA RAM", win_x + 15, win_y + 11, COLOR_WHITE);
        else if (active_app == 2) gfx_draw_string("JANELA: DIAGNOSTICO IDT & CPU", win_x + 15, win_y + 11, COLOR_WHITE);

        gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);
        gfx_draw_rect(win_x + win_w - 45, win_y + 7, 16, 16, COLOR_BLUE);

        if (active_app == 0) {
            gfx_draw_string("TERMINAL SHELL INTERATIVO:", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_string("DIGITE UM COMANDO (HELP, CLEAR, RAM, PANIC):", win_x + 30, win_y + 80, COLOR_WHITE);

            gfx_draw_rect(win_x + 30, win_y + 105, win_w - 60, 35, COLOR_NAVY);
            gfx_draw_string("myos> ", win_x + 40, win_y + 118, COLOR_GREEN);
            gfx_draw_string(input_buffer, win_x + 90, win_y + 118, COLOR_WHITE);

            gfx_draw_rect(win_x + 30, win_y + 160, win_w - 60, 240, COLOR_NAVY);
            gfx_draw_string("SAIDA DO CONSOLE:", win_x + 50, win_y + 180, COLOR_BLUE);
            gfx_draw_string(shell_output, win_x + 50, win_y + 220, COLOR_WHITE);
        } else if (active_app == 1) {
            gfx_draw_string("GERENCIADOR DE MEMORIA HEAP:", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 300, COLOR_NAVY);
            gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 120, COLOR_WHITE);
            gfx_draw_number((int)memory_get_total_allocated(), win_x + 220, win_y + 120, COLOR_GREEN);
            gfx_draw_string(" BYTES", win_x + 290, win_y + 120, COLOR_WHITE);

            gfx_draw_string("RAM LIVRE DISPONIVEL: ", win_x + 50, win_y + 160, COLOR_WHITE);
            gfx_draw_number((int)memory_get_total_free() / 1024, win_x + 250, win_y + 160, COLOR_GREEN);
            gfx_draw_string(" KB", win_x + 330, win_y + 160, COLOR_WHITE);

            gfx_draw_string("TOTAL BLOCOS HEAP: ", win_x + 50, win_y + 200, COLOR_WHITE);
            gfx_draw_number((int)memory_get_block_count(), win_x + 220, win_y + 200, COLOR_WHITE);
        } else if (active_app == 2) {
            gfx_draw_string("DIAGNOSTICO DE INTERRUPCOES & CPU:", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 300, COLOR_NAVY);
            gfx_draw_string("STATUS DA TABELA IDT: ATIVA (256 GATES)", win_x + 50, win_y + 120, COLOR_WHITE);
            gfx_draw_string("DRIVER TECLADO PS/2: IRQ1 (INT 33) OK", win_x + 50, win_y + 160, COLOR_WHITE);
            gfx_draw_string("DRIVER MOUSE PS/2: IRQ12 (INT 44) OK", win_x + 50, win_y + 200, COLOR_WHITE);
            gfx_draw_string("RELOGIO DE HARDWARE (CMOS RTC): ATIVO (BRT)", win_x + 50, win_y + 240, COLOR_GREEN);
        }
    }

    // MENU START POP-UP
    if (start_menu_open) {
        gfx_draw_rect(10, 430, 190, 130, COLOR_NAVY);
        gfx_draw_rect(10, 430, 190, 25, COLOR_BLUE);
        gfx_draw_string("MENU START", 20, 438, COLOR_WHITE);

        gfx_draw_string("> 1. SHELL (CLI)", 20, 465, active_app == 0 ? COLOR_GREEN : COLOR_WHITE);
        gfx_draw_string("> 2. MEMORIA (RAM)", 20, 500, active_app == 1 ? COLOR_GREEN : COLOR_WHITE);
        gfx_draw_string("> 3. IDT / HARDWARE", 20, 535, active_app == 2 ? COLOR_GREEN : COLOR_WHITE);
    }

    gfx_draw_cursor(mouse_x, mouse_y);
    gfx_swap_buffers();
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL BARE-METAL v0.6\n");

    memory_init(mbi);
    gfx_init(mbi);
    idt_init();

    input_buffer[0] = '\0';

    while (1) {
        handle_mouse_events();

        if (last_key_pressed != 0) {
            char c = last_key_pressed;
            last_key_pressed = 0;

            if (c == '\b') {
                if (input_index > 0) input_buffer[--input_index] = '\0';
            } else if (c == '\n') {
                if (active_app == 0) process_shell_command();
            } else if (input_index < 30) {
                input_buffer[input_index++] = c;
                input_buffer[input_index] = '\0';
            }
        }

        render_gui();
        asm volatile ("hlt");
    }
}
