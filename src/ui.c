#include "../include/ui.h"
#include "../include/gfx.h"
#include "../include/util.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/rtc.h"
#include "../include/vfs.h"
#include "../include/sound.h"
#include "../include/music.h"
#include "../include/bmp.h"
#include "../include/task.h"
#include "../include/elf.h"
#include "../include/net.h"

#define MAX_WINDOWS 9

typedef struct {
    int id; int app_type; int x, y, w, h; int is_open; int z_index;
} window_t;

static window_t windows[MAX_WINDOWS];
static int top_z_index = 0, dragging_window_id = -1, drag_off_x = 0, drag_off_y = 0;
static int current_wallpaper = -1, start_menu_open = 0, gallery_photo = 0;
static char gallery_status_msg[64] = "CLIQUE NOS BOTOES PARA ALTERAR WALLPAPER OU SALVAR!";

static char input_buffer[32]; static int input_index = 0;
static char shell_output[128] = "SISTEMA VFS & RING 3 SYSCALLS PRONTOS!";
static uint32_t paint_canvas[160 * 120]; static uint32_t paint_color = 0xFFFFFF;
static int prev_mouse_left = 0;

// ESTRUTURA E FÍSICA DO JOGO SNAKE
static int snake_body_x[100];
static int snake_body_y[100];
static int snake_len = 3;
static int snake_dir_x = 5, snake_dir_y = 0;
static int food_x = 200, food_y = 150;
static int snake_score = 0;
static int snake_game_over = 0;

static void reset_snake(int canvas_x, int canvas_y) {
    snake_len = 3; snake_dir_x = 5; snake_dir_y = 0;
    snake_score = 0; snake_game_over = 0;
    for (int i = 0; i < snake_len; i++) {
        snake_body_x[i] = canvas_x + 60 - (i * 8);
        snake_body_y[i] = canvas_y + 50;
    }
    food_x = canvas_x + 150; food_y = canvas_y + 100;
}

static void format_time_string(char* buf, uint8_t h, uint8_t m, uint8_t s) {
    buf[0] = '0' + (h / 10); buf[1] = '0' + (h % 10); buf[2] = ':';
    buf[3] = '0' + (m / 10); buf[4] = '0' + (m % 10); buf[5] = ':';
    buf[6] = '0' + (s / 10); buf[7] = '0' + (s % 10); buf[8] = ' ';
    buf[9] = 'B'; buf[10] = 'R'; buf[11] = 'T'; buf[12] = '\0';
}

void bring_to_front(int win_id) {
    top_z_index++; windows[win_id].z_index = top_z_index; windows[win_id].is_open = 1;
}

void ui_init(void) {
    music_init(); net_init();
    kmemset(paint_canvas, 0, sizeof(paint_canvas));
    input_buffer[0] = '\0';

    windows[0] = (window_t){0, 0, 180, 40, 580, 480, 0, 1}; // Shell
    windows[1] = (window_t){1, 1, 140, 60, 580, 480, 0, 2}; // Memoria
    windows[2] = (window_t){2, 2, 100, 80, 580, 480, 0, 3}; // IDT & Net
    windows[3] = (window_t){3, 3, 120, 30, 580, 500, 0, 4}; // Galeria
    windows[4] = (window_t){4, 4, 220, 70, 520, 440, 0, 5}; // Musica
    windows[5] = (window_t){5, 5, 160, 40, 600, 460, 0, 6}; // Tarefas
    windows[6] = (window_t){6, 6, 90,  50, 540, 480, 0, 7}; // Paint
    windows[7] = (window_t){7, 7, 200, 80, 500, 380, 0, 8}; // Explorador #|
    windows[8] = (window_t){8, 8, 110, 50, 520, 440, 1, 9}; // Snake Ring 3 (Abre Primeiro!)

    top_z_index = 9;
    reset_snake(110 + 30, 50 + 85);
}

static void process_shell_command(void) {
    for (int i = 0; i < 128; i++) shell_output[i] = '\0';

    if (kstrcmp(input_buffer, "help") == 0) {
        const char* msg = "COMANDOS: LS, CAT <ARQ>, WRITE <ARQ> <TEXTO>, RUN <ELF>, PANIC";
        for (int i = 0; msg[i] != '\0'; i++) shell_output[i] = msg[i];
    } else if (kstrcmp(input_buffer, "ls") == 0) {
        vfs_list(shell_output, 128);
    } else if (kstrncmp(input_buffer, "cat ", 4) == 0) {
        size_t sz = 0;
        const uint8_t* content = vfs_read(input_buffer + 4, &sz);
        if (content) {
            for (size_t i = 0; i < sz && i < 120; i++) shell_output[i] = content[i];
        }
    } else if (kstrncmp(input_buffer, "write ", 6) == 0) {
        char filename[32]; int f_idx = 0, i = 6;
        while (input_buffer[i] != ' ' && input_buffer[i] != '\0' && f_idx < 31) filename[f_idx++] = input_buffer[i++];
        filename[f_idx] = '\0';
        if (input_buffer[i] == ' ') i++;
        if (vfs_write_file(filename, (const uint8_t*)(input_buffer + i), kstrlen(input_buffer + i))) {
            const char* msg = "ARQUIVO GRAVADO COM SUCESSO!";
            for (int j = 0; msg[j] != '\0'; j++) shell_output[j] = msg[j];
        }
    } else if (kstrcmp(input_buffer, "clear") == 0) {
        shell_output[0] = '\0';
    }
    input_index = 0; input_buffer[0] = '\0';
}

void ui_handle_keyboard(void) {
    if (last_key_pressed != 0) {
        char c = last_key_pressed; last_key_pressed = 0;

        if (snake_game_over) {
            reset_snake(windows[8].x + 30, windows[8].y + 85);
            return;
        }

        if (c == 'w' && snake_dir_y == 0) { snake_dir_x = 0; snake_dir_y = -5; }
        else if (c == 's' && snake_dir_y == 0) { snake_dir_x = 0; snake_dir_y = 5; }
        else if (c == 'a' && snake_dir_x == 0) { snake_dir_x = -5; snake_dir_y = 0; }
        else if (c == 'd' && snake_dir_x == 0) { snake_dir_x = 5; snake_dir_y = 0; }

        if (c == '\b') { if (input_index > 0) input_buffer[--input_index] = '\0'; }
        else if (c == '\n') process_shell_command();
        else if (input_index < 30) { input_buffer[input_index++] = c; input_buffer[input_index] = '\0'; }
    }
}

int find_clicked_window(int mx, int my) {
    int top_id = -1, highest_z = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].is_open && mx >= windows[i].x && mx <= (windows[i].x + windows[i].w) &&
            my >= windows[i].y && my <= (windows[i].y + windows[i].h)) {
            if (windows[i].z_index > highest_z) { highest_z = windows[i].z_index; top_id = i; }
        }
    }
    return top_id;
}

void ui_handle_mouse(void) {
    int click = mouse_left_clicked;
    int just_pressed = (click && !prev_mouse_left);
    prev_mouse_left = click;

    if (just_pressed) sound_click();

    // 1. CLIQUE NOS 8 ÍCONES DO DESKTOP
    if (just_pressed && mouse_x >= 15 && mouse_x <= 135) {
        if (mouse_y >= 20 && mouse_y <= 75) bring_to_front(0);        // Shell VFS
        else if (mouse_y >= 80 && mouse_y <= 135) bring_to_front(1);  // Memoria
        else if (mouse_y >= 140 && mouse_y <= 195) bring_to_front(2); // IDT CPU
        else if (mouse_y >= 200 && mouse_y <= 255) bring_to_front(3); // Galeria
        else if (mouse_y >= 260 && mouse_y <= 315) bring_to_front(4); // Musica
        else if (mouse_y >= 320 && mouse_y <= 375) bring_to_front(5); // Tarefas
        else if (mouse_y >= 380 && mouse_y <= 435) bring_to_front(6); // Paint
        else if (mouse_y >= 440 && mouse_y <= 495) bring_to_front(7); // Explorador #|
    }

    // 2. Botão START
    if (just_pressed && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= 565 && mouse_y <= 595) {
        start_menu_open = !start_menu_open; return;
    }

    // 3. Menu Start
    if (start_menu_open && just_pressed && mouse_x >= 10 && mouse_x <= 200 && mouse_y >= 280 && mouse_y <= 560) {
        if (mouse_y >= 290 && mouse_y < 325) bring_to_front(0);
        else if (mouse_y >= 325 && mouse_y < 360) bring_to_front(1);
        else if (mouse_y >= 360 && mouse_y < 395) bring_to_front(2);
        else if (mouse_y >= 395 && mouse_y < 430) bring_to_front(3);
        else if (mouse_y >= 430 && mouse_y < 465) bring_to_front(4);
        else if (mouse_y >= 465 && mouse_y < 500) bring_to_front(5);
        else if (mouse_y >= 500 && mouse_y < 535) bring_to_front(6);
        else if (mouse_y >= 535 && mouse_y <= 560) bring_to_front(7);
        start_menu_open = 0; return;
    }

    if (start_menu_open && just_pressed) start_menu_open = 0;

    if (just_pressed) {
        int clicked_win = find_clicked_window(mouse_x, mouse_y);
        if (clicked_win != -1) {
            bring_to_front(clicked_win);
            window_t* w = &windows[clicked_win];

            if (mouse_x >= (w->x + w->w - 25) && mouse_x <= (w->x + w->w - 9) &&
                mouse_y >= (w->y + 7) && mouse_y <= (w->y + 23)) {
                w->is_open = 0; dragging_window_id = -1; return;
            }

            if (mouse_y >= w->y && mouse_y <= (w->y + 30)) {
                dragging_window_id = clicked_win; drag_off_x = mouse_x - w->x; drag_off_y = mouse_y - w->y;
            }

            if (w->app_type == 4 && mouse_y >= w->y + 160 && mouse_y <= w->y + 200) {
                if (mouse_x >= w->x + 50 && mouse_x <= w->x + 200) music_toggle_play();
                else if (mouse_x >= w->x + 220 && mouse_x <= w->x + 400) music_next_track();
            }

            if (w->app_type == 3) {
                int btn_y1 = w->y + 390; int btn_y2 = w->y + 430;
                if (mouse_y >= btn_y1 && mouse_y <= btn_y1 + 30) {
                    if (mouse_x >= w->x + 20 && mouse_x <= w->x + 110) { gallery_photo = 0; kstrcpy(gallery_status_msg, "POR DO SOL GERADO!"); }
                    else if (mouse_x >= w->x + 120 && mouse_x <= w->x + 200) { gallery_photo = 1; kstrcpy(gallery_status_msg, "GALAXIA GERADA!"); }
                    else if (mouse_x >= w->x + 210 && mouse_x <= w->x + 310) { gallery_photo = 2; kstrcpy(gallery_status_msg, "SYNTHWAVE GERADO!"); }
                    else if (mouse_x >= w->x + 320 && mouse_x <= w->x + 450) { gallery_photo = 3; kstrcpy(gallery_status_msg, "ABRINDO FOTO.BMP DO DISCO..."); }
                }

                if (mouse_y >= btn_y2 && mouse_y <= btn_y2 + 30) {
                    if (mouse_x >= w->x + 20 && mouse_x <= w->x + 210) current_wallpaper = gallery_photo;
                    else if (mouse_x >= w->x + 225 && mouse_x <= w->x + 435) vfs_write_file("paisagem.art", (const uint8_t*)"ARTE RECENTE DA GALERIA", 23);
                    else if (mouse_x >= w->x + 450 && mouse_x <= w->x + 610) current_wallpaper = -1;
                }
            }

            if (w->app_type == 6 && mouse_y >= w->y + 430 && mouse_y <= w->y + 460) {
                if (mouse_x >= w->x + 20 && mouse_x <= w->x + 60) paint_color = 0xFF0000;
                else if (mouse_x >= w->x + 70 && mouse_x <= w->x + 110) paint_color = 0x00FF00;
                else if (mouse_x >= w->x + 120 && mouse_x <= w->x + 160) paint_color = 0x0000FF;
                else if (mouse_x >= w->x + 170 && mouse_x <= w->x + 210) paint_color = 0xFFFFFF;
                else if (mouse_x >= w->x + 220 && mouse_x <= w->x + 260) paint_color = 0x000000;
                else if (mouse_x >= w->x + 310 && mouse_x <= w->x + 510) bmp_export("arte.bmp", paint_canvas, 160, 120);
            }
        }
    }

    if (click) {
        int paint_win = find_clicked_window(mouse_x, mouse_y);
        if (paint_win != -1 && windows[paint_win].app_type == 6) {
            window_t* w = &windows[paint_win];
            int c_x = w->x + 30, c_y = w->y + 60;
            if (mouse_x >= c_x && mouse_x < c_x + 480 && mouse_y >= c_y && mouse_y < c_y + 360) {
                int p_x = (mouse_x - c_x) / 3;
                int p_y = (mouse_y - c_y) / 3;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (p_x + dx >= 0 && p_x + dx < 160 && p_y + dy >= 0 && p_y + dy < 120) {
                            paint_canvas[(p_y + dy) * 160 + (p_x + dx)] = paint_color;
                        }
                    }
                }
            }
        }
    }

    if (click && dragging_window_id != -1) {
        window_t* w = &windows[dragging_window_id];
        w->x = mouse_x - drag_off_x; w->y = mouse_y - drag_off_y;
        if (w->x < 0) w->x = 0; if (w->x + w->w > 800) w->x = 800 - w->w;
        if (w->y < 0) w->y = 0; if (w->y + w->h > 555) w->y = 555 - w->h;
    }

    if (!click) dragging_window_id = -1;
}

void draw_single_window(window_t* w) {
    int win_x = w->x, win_y = w->y, win_w = w->w, win_h = w->h;

    gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B);
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);
    gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);

    if (w->app_type == 0) gfx_draw_string("JANELA: TERMINAL SHELL VFS", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 1) gfx_draw_string("JANELA: GERENCIADOR DE MEMORIA RAM", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 2) gfx_draw_string("JANELA: DIAGNOSTICO REDE RTL8139 & IDT", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 3) gfx_draw_string("JANELA: GALERIA DE PAISAGENS & .BMP", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 4) gfx_draw_string("JANELA: PLAYER DE MUSICA CHIPTUNE 8-BIT", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 5) gfx_draw_string("JANELA: GERENCIADOR DE TAREFAS DA CPU", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 6) gfx_draw_string("JANELA: PAINT STUDIO & EXPORTADOR BMP", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 7) gfx_draw_string("JANELA: EXPLORADOR DE ARQUIVOS (NOTACAO #|)", win_x + 15, win_y + 11, COLOR_WHITE);
    else if (w->app_type == 8) gfx_draw_string("JANELA: JOGO SNAKE EM RING 3 (SYSCALLS)", win_x + 15, win_y + 11, COLOR_WHITE);

    gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);

    if (w->app_type == 0) {
        gfx_draw_string("TERMINAL SHELL VFS:", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 105, win_w - 60, 35, COLOR_NAVY);
        gfx_draw_string("myos> ", win_x + 40, win_y + 118, COLOR_GREEN);
        gfx_draw_string(input_buffer, win_x + 90, win_y + 118, COLOR_WHITE);
        gfx_draw_rect(win_x + 30, win_y + 160, win_w - 60, 240, COLOR_NAVY);
        gfx_draw_string(shell_output, win_x + 50, win_y + 220, COLOR_WHITE);
    } else if (w->app_type == 1) {
        gfx_draw_string("GERENCIADOR DE MEMORIA HEAP (RAM):", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 300, COLOR_NAVY);
        gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 120, COLOR_WHITE);
        gfx_draw_number((int)memory_get_total_allocated(), win_x + 220, win_y + 120, COLOR_GREEN);
        gfx_draw_string(" BYTES", win_x + 290, win_y + 120, COLOR_WHITE);
    } else if (w->app_type == 2) {
        gfx_draw_string("DIAGNOSTICO DA PLACA DE REDE REALTEK RTL8139:", win_x + 30, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 70, win_w - 60, 350, COLOR_NAVY);
        gfx_draw_string("STATUS DA PLACA: REALTEK RTL8139 (PCI) - ONLINE", win_x + 50, win_y + 100, COLOR_GREEN);
        gfx_draw_string("ENDERECO IP DO SO: 10.0.2.15 (QEMU Net)", win_x + 50, win_y + 140, COLOR_WHITE);
    } else if (w->app_type == 3) {
        int canvas_x = win_x + 30, canvas_y = win_y + 65, canvas_w = 520, canvas_h = 310;
        if (gallery_photo == 0) gfx_draw_landscape_sunset(canvas_x, canvas_y, canvas_w, canvas_h);
        else if (gallery_photo == 1) gfx_draw_landscape_cosmos(canvas_x, canvas_y, canvas_w, canvas_h);
        else if (gallery_photo == 2) gfx_draw_landscape_synthwave(canvas_x, canvas_y, canvas_w, canvas_h);
        else if (gallery_photo == 3) {
            gfx_draw_rect(canvas_x, canvas_y, canvas_w, canvas_h, COLOR_NAVY);
            size_t sz = 0;
            const uint8_t* bmp_bytes = vfs_read("foto.bmp", &sz);
            if (bmp_bytes) bmp_draw(bmp_bytes, canvas_x, canvas_y, canvas_w, canvas_h);
        }

        int btn_y1 = win_y + 390;
        gfx_draw_rect(win_x + 20, btn_y1, 90, 30, gallery_photo == 0 ? COLOR_BLUE : COLOR_NAVY);
        gfx_draw_string("POR DO SOL", win_x + 25, btn_y1 + 11, COLOR_WHITE);
        gfx_draw_rect(win_x + 120, btn_y1, 80, 30, gallery_photo == 1 ? COLOR_BLUE : COLOR_NAVY);
        gfx_draw_string("GALAXIA", win_x + 125, btn_y1 + 11, COLOR_WHITE);
        gfx_draw_rect(win_x + 210, btn_y1, 100, 30, gallery_photo == 2 ? COLOR_BLUE : COLOR_NAVY);
        gfx_draw_string("SYNTHWAVE", win_x + 215, btn_y1 + 11, COLOR_WHITE);
        gfx_draw_rect(win_x + 320, btn_y1, 130, 30, gallery_photo == 3 ? COLOR_BLUE : COLOR_NAVY);
        gfx_draw_string("4. ABRIR .BMP", win_x + 325, btn_y1 + 11, COLOR_GREEN);
    } else if (w->app_type == 4) {
        gfx_draw_string("PLAYER DE MUSICA CHIPTUNE 8-BIT:", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 320, COLOR_NAVY);
        gfx_draw_string("FAIXA ATUAL:", win_x + 50, win_y + 120, COLOR_WHITE);
        gfx_draw_string(music_get_track_name(), win_x + 180, win_y + 120, COLOR_BLUE);
    } else if (w->app_type == 5) {
        gfx_draw_string("GERENCIADOR DE TAREFAS (ROUND-ROBIN MULTITASKING):", win_x + 30, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 70, win_w - 60, 310, COLOR_NAVY);
        gfx_draw_string("PID   NOME              PRIORIDADE   ESTADO      TICKS", win_x + 40, win_y + 85, COLOR_BLUE);
    } else if (w->app_type == 6) {
        gfx_draw_string("APP PAINT - DESENHE E EXPORTE PARA .BMP:", win_x + 30, win_y + 42, COLOR_GREEN);
        int cx = win_x + 30, cy = win_y + 60;
        for (int py = 0; py < 360; py++) {
            for (int px = 0; px < 480; px++) {
                uint32_t color = paint_canvas[(py / 3) * 160 + (px / 3)];
                gfx_put_pixel(cx + px, cy + py, color);
            }
        }
    } else if (w->app_type == 7) {
        gfx_draw_string("EXPLORADOR DE ARQUIVOS (SINTAXE CUSTOMIZADA #|):", win_x + 30, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 70, win_w - 60, 30, COLOR_NAVY);
        gfx_draw_string("CAMINHO ATUAL: #|ROOT*", win_x + 40, win_y + 78, COLOR_YELLOW);
        gfx_draw_rect(win_x + 30, win_y + 110, win_w - 60, 240, COLOR_NAVY);
        char custom_vfs_list[256];
        vfs_list_custom_format(custom_vfs_list, 256);
        gfx_draw_string(custom_vfs_list, win_x + 50, win_y + 130, COLOR_WHITE);
    } else if (w->app_type == 8) {
        // JOGO SNAKE AUTÔNOMO COM DETECÇÃO DE COLISÃO
        gfx_draw_string("JOGO SNAKE EM RING 3 (TESTE DE SYSCALLS):", win_x + 30, win_y + 45, COLOR_GREEN);

        int canvas_x = win_x + 30, canvas_y = win_y + 70, canvas_w = 460, canvas_h = 300;
        gfx_draw_rect(canvas_x, canvas_y, canvas_w, canvas_h, COLOR_NAVY);

        if (snake_game_over) {
            gfx_draw_string("GAME OVER! COLISAO COM PAREDE OU CORPO!", canvas_x + 40, canvas_y + 120, COLOR_RED);
            gfx_draw_string("APERTE QUALQUER TECLA (W/A/S/D) PARA REINICIAR", canvas_x + 20, canvas_y + 160, COLOR_WHITE);
        } else {
            // MOVIMENTO AUTOMÁTICO
            for (int i = snake_len - 1; i > 0; i--) {
                snake_body_x[i] = snake_body_x[i - 1];
                snake_body_y[i] = snake_body_y[i - 1];
            }
            snake_body_x[0] += snake_dir_x;
            snake_body_y[0] += snake_dir_y;

            int head_x = snake_body_x[0];
            int head_y = snake_body_y[0];

            // 1. CHECAGEM DE COLISÃO COM PAREDES
            if (head_x < canvas_x + 2 || head_x >= canvas_x + canvas_w - 12 ||
                head_y < canvas_y + 2 || head_y >= canvas_y + canvas_h - 12) {
                snake_game_over = 1;
                sound_play(150); // Som grave de explosão/game over
            }

            // 2. CHECAGEM DE COLISÃO COM O PRÓPRIO CORPO
            for (int i = 1; i < snake_len; i++) {
                if (head_x == snake_body_x[i] && head_y == snake_body_y[i]) {
                    snake_game_over = 1;
                    sound_play(150);
                    break;
                }
            }

            // 3. CHECAGEM DE COLISÃO COM A FRUTA
            if (head_x >= food_x - 10 && head_x <= food_x + 10 &&
                head_y >= food_y - 10 && head_y <= food_y + 10) {
                if (snake_len < 99) snake_len++;
                snake_score += 10;
                food_x = canvas_x + 20 + ((snake_score * 37) % (canvas_w - 40));
                food_y = canvas_y + 20 + ((snake_score * 19) % (canvas_h - 40));
                sound_play(1200); // Beep de comida
            }

            // DESENHA A FRUTINHA
            gfx_draw_rect(food_x, food_y, 10, 10, COLOR_RED);

            // DESENHA O CORPO DA COBRINHA
            for (int i = 0; i < snake_len; i++) {
                gfx_draw_rect(snake_body_x[i], snake_body_y[i], 8, 8, i == 0 ? COLOR_YELLOW : COLOR_GREEN);
            }
        }

        gfx_draw_string("PONTUACAO: ", win_x + 30, win_y + 390, COLOR_WHITE);
        gfx_draw_number(snake_score, win_x + 120, win_y + 390, COLOR_GREEN);
        gfx_draw_string("INSTRUCAO SYSCALL DE 64-BITS EXEC ATIVA!", win_x + 200, win_y + 390, COLOR_BLUE);
    }
}

void draw_desktop_icon(int x, int y, int app_id, const char* title, const char* sub, uint32_t color) {
    int is_hovered = (mouse_x >= x && mouse_x <= x + 115 && mouse_y >= y && mouse_y <= y + 50);
    uint32_t bg_color = is_hovered ? COLOR_BLUE : COLOR_NAVY;
    uint32_t border_color = is_hovered ? COLOR_GREEN : COLOR_GRAY;

    gfx_draw_rect(x, y, 115, 50, bg_color);
    gfx_draw_rect(x + 3, y + 3, 109, 14, border_color);
    gfx_draw_string(title, x + 10, y + 6, COLOR_WHITE);
    gfx_draw_string(sub, x + 10, y + 28, color);
}

void ui_render(void) {
    if (current_wallpaper == 0) gfx_draw_landscape_sunset(0, 0, 800, 600);
    else if (current_wallpaper == 1) gfx_draw_landscape_cosmos(0, 0, 800, 600);
    else if (current_wallpaper == 2) gfx_draw_landscape_synthwave(0, 0, 800, 600);
    else gfx_clear(COLOR_DARK_SLATE);

    // ÍCONES DA ÁREA DE TRABALHO COM EFEITO HOVER MOUSE HIGHLIGHT
    int ic_x = 15;
    draw_desktop_icon(ic_x, 20,  0, "1. SHELL",    "DISCO VFS", COLOR_GREEN);
    draw_desktop_icon(ic_x, 80,  1, "2. MEMORIA",  "RAM HEAP", COLOR_GREEN);
    draw_desktop_icon(ic_x, 140, 2, "3. REDE/PING", "RTL8139",   COLOR_GREEN);
    draw_desktop_icon(ic_x, 200, 3, "4. GALERIA",  "PINTOR/BMP",COLOR_GREEN);
    draw_desktop_icon(ic_x, 260, 4, "5. MUSICA",   "CHIPTUNE",  COLOR_GREEN);
    draw_desktop_icon(ic_x, 320, 5, "6. TAREFAS",  "MULTITASK", COLOR_WHITE);
    draw_desktop_icon(ic_x, 380, 6, "7. PAINT",    "BMP ESTUDIO",COLOR_WHITE);
    draw_desktop_icon(ic_x, 440, 7, "8. EXPLORAR", "SINTAXE #|",COLOR_YELLOW);

    // BARRA DE TAREFAS
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, start_menu_open ? COLOR_GREEN : COLOR_BLUE);
    gfx_draw_string("START", 30, 576, COLOR_NAVY);

    rtc_time_t clock_brt;
    rtc_get_time_brt(&clock_brt);
    char time_str[16];
    format_time_string(time_str, clock_brt.hour, clock_brt.minute, clock_brt.second);
    gfx_draw_string(time_str, 680, 576, COLOR_WHITE);

    for (int z = 1; z <= top_z_index; z++) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z_index == z) draw_single_window(&windows[i]);
        }
    }

    if (start_menu_open) {
        gfx_draw_rect(10, 280, 190, 280, COLOR_NAVY);
        gfx_draw_rect(10, 280, 190, 25, COLOR_BLUE);
        gfx_draw_string("MENU START", 20, 288, COLOR_WHITE);

        gfx_draw_string("> 1. SHELL VFS", 20, 315, COLOR_WHITE);
        gfx_draw_string("> 2. MEMORIA (RAM)", 20, 350, COLOR_WHITE);
        gfx_draw_string("> 3. IDT / HARDWARE", 20, 385, COLOR_WHITE);
        gfx_draw_string("> 4. GALERIA (PINTOR)", 20, 420, COLOR_WHITE);
        gfx_draw_string("> 5. PLAYER DE MUSICA", 20, 455, COLOR_WHITE);
        gfx_draw_string("> 6. TAREFAS (CPU)", 20, 490, COLOR_WHITE);
        gfx_draw_string("> 7. PAINT STUDIO", 20, 525, COLOR_WHITE);
    }

    gfx_draw_cursor(mouse_x, mouse_y);
    gfx_swap_buffers();
}
