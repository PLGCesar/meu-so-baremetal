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
    int id;
    int app_type;
    int x, y, w, h;
    int is_open;
    int z_index;
    int anim_scale; // ANIMAÇÃO SUAVE DE ZOOM (0% A 100%)
} window_t;

static window_t windows[MAX_WINDOWS];
static int top_z_index = 0, dragging_window_id = -1, drag_off_x = 0, drag_off_y = 0;
static int current_wallpaper = -1, start_menu_open = 0, gallery_photo = 0;
static char gallery_status_msg[64] = "CLIQUE NOS BOTOES PARA ALTERAR WALLPAPER OU SALVAR!";

static char input_buffer[32]; static int input_index = 0;
static char shell_output[128] = "SYSTEM VFS & RING 3 SYSCALLS PRONTOS!";
static uint32_t paint_canvas[160 * 120]; static uint32_t paint_color = 0xFFFFFF;
static int prev_mouse_left = 0;

static int snake_x = 100, snake_y = 100;
static int snake_dir_x = 5, snake_dir_y = 0;
static int food_x = 200, food_y = 150;
static int snake_score = 0;

static void format_time_string(char* buf, uint8_t h, uint8_t m, uint8_t s) {
    buf[0] = '0' + (h / 10); buf[1] = '0' + (h % 10); buf[2] = ':';
    buf[3] = '0' + (m / 10); buf[4] = '0' + (m % 10); buf[5] = ':';
    buf[6] = '0' + (s / 10); buf[7] = '0' + (s % 10); buf[8] = ' ';
    buf[9] = 'B'; buf[10] = 'R'; buf[11] = 'T'; buf[12] = '\0';
}

void bring_to_front(int win_id) {
    top_z_index++;
    windows[win_id].z_index = top_z_index;
    if (!windows[win_id].is_open) {
        windows[win_id].anim_scale = 10; // Inicia Animação de Zoom!
    }
    windows[win_id].is_open = 1;
}

void ui_init(void) {
    music_init(); net_init();
    kmemset(paint_canvas, 0, sizeof(paint_canvas));
    input_buffer[0] = '\0';

    // Ajustado para resolução HD 1024x768
    windows[0] = (window_t){0, 0, 240, 60, 640, 520, 0, 1, 100}; // Shell
    windows[1] = (window_t){1, 1, 200, 80, 640, 520, 0, 2, 100}; // Memoria
    windows[2] = (window_t){2, 2, 160, 100, 640, 520, 0, 3, 100}; // IDT & Net
    windows[3] = (window_t){3, 3, 180, 50, 640, 540, 0, 4, 100}; // Galeria
    windows[4] = (window_t){4, 4, 260, 90, 580, 480, 0, 5, 100}; // Musica
    windows[5] = (window_t){5, 5, 200, 60, 660, 500, 0, 6, 100}; // Tarefas
    windows[6] = (window_t){6, 6, 130, 70, 600, 520, 0, 7, 100}; // Paint
    windows[7] = (window_t){7, 7, 240, 100, 560, 420, 1, 8, 100}; // Explorador #| (Abre primeiro!)
    windows[8] = (window_t){8, 8, 150, 70, 580, 480, 0, 9, 100}; // Snake Ring 3

    top_z_index = 9;
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
        if (windows[i].is_open) {
            if (mx >= windows[i].x && mx <= (windows[i].x + windows[i].w) &&
                my >= windows[i].y && my <= (windows[i].y + windows[i].h)) {
                if (windows[i].z_index > highest_z) { highest_z = windows[i].z_index; top_id = i; }
            }
        }
    }
    return top_id;
}

void ui_handle_mouse(void) {
    int click = mouse_left_clicked;
    int just_pressed = (click && !prev_mouse_left);
    prev_mouse_left = click;

    if (just_pressed) sound_click();

    // 1. ÍCONES DO DESKTOP EM HD 1024X768
    if (just_pressed && mouse_x >= 15 && mouse_x <= 135) {
        if (mouse_y >= 20 && mouse_y <= 75) bring_to_front(0);
        else if (mouse_y >= 80 && mouse_y <= 135) bring_to_front(1);
        else if (mouse_y >= 140 && mouse_y <= 195) bring_to_front(2);
        else if (mouse_y >= 200 && mouse_y <= 255) bring_to_front(3);
        else if (mouse_y >= 260 && mouse_y <= 315) bring_to_front(4);
        else if (mouse_y >= 320 && mouse_y <= 375) bring_to_front(5);
        else if (mouse_y >= 380 && mouse_y <= 435) bring_to_front(6);
        else if (mouse_y >= 440 && mouse_y <= 495) bring_to_front(7);
    }

    // 2. Botão START na barra inferior HD (Y = 728)
    if (just_pressed && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= 733 && mouse_y <= 763) {
        start_menu_open = !start_menu_open; return;
    }

    // 3. Menu Start Pop-up
    if (start_menu_open && just_pressed && mouse_x >= 10 && mouse_x <= 200 && mouse_y >= 440 && mouse_y <= 728) {
        if (mouse_y >= 450 && mouse_y < 485) bring_to_front(0);
        else if (mouse_y >= 485 && mouse_y < 520) bring_to_front(1);
        else if (mouse_y >= 520 && mouse_y < 555) bring_to_front(2);
        else if (mouse_y >= 555 && mouse_y < 590) bring_to_front(3);
        else if (mouse_y >= 590 && mouse_y < 625) bring_to_front(4);
        else if (mouse_y >= 625 && mouse_y < 660) bring_to_front(5);
        else if (mouse_y >= 660 && mouse_y < 695) bring_to_front(6);
        else if (mouse_y >= 695 && mouse_y <= 725) bring_to_front(7);
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
        if (w->x < 0) w->x = 0; if (w->x + w->w > 1024) w->x = 1024 - w->w;
        if (w->y < 0) w->y = 0; if (w->y + w->h > 720) w->y = 720 - w->h;
    }

    if (!click) dragging_window_id = -1;
}

void draw_single_window(window_t* w) {
    // INTERPOLAÇÃO DE ANIMAÇÃO DE ZOOM NA ABERTURA DA JANELA (EASING)
    if (w->anim_scale < 100) {
        w->anim_scale += 25; // Expande a janela em 4 quadros suaves!
        if (w->anim_scale > 100) w->anim_scale = 100;
    }

    int base_w = w->w;
    int base_h = w->h;
    int cur_w = (base_w * w->anim_scale) / 100;
    int cur_h = (base_h * w->anim_scale) / 100;

    int win_x = w->x + (base_w - cur_w) / 2;
    int win_y = w->y + (base_h - cur_h) / 2;
    int win_w = cur_w;
    int win_h = cur_h;

    // Sombra Translúcida com Alpha Blending!
    gfx_draw_rect_alpha(win_x + 8, win_y + 8, win_w, win_h, 0x000000, 100);
    
    // Corpo da Janela
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);

    // BARRA DE TÍTULO TRANSPARENTE EFEITO VIDRO (85% OPACIDADE)
    gfx_draw_rect_alpha(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY, 210);

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

    if (w->anim_scale < 100) return; // Aguarda o fim do zoom para desenhar conteúdo interno

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

        int btn_y2 = win_y + 430;
        gfx_draw_rect(win_x + 20, btn_y2, 190, 30, COLOR_BLUE);
        gfx_draw_string("USAR COMO WALLPAPER", win_x + 30, btn_y2 + 11, COLOR_WHITE);
        gfx_draw_rect(win_x + 225, btn_y2, 210, 30, COLOR_GREEN);
        gfx_draw_string("SALVAR NO DISCO VFS", win_x + 245, btn_y2 + 11, COLOR_NAVY);

        gfx_draw_string(gallery_status_msg, win_x + 30, win_y + 475, COLOR_WHITE);
    } else if (w->app_type == 4) {
        gfx_draw_string("PLAYER DE MUSICA CHIPTUNE 8-BIT:", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 320, COLOR_NAVY);
        gfx_draw_string("FAIXA ATUAL:", win_x + 50, win_y + 120, COLOR_WHITE);
        gfx_draw_string(music_get_track_name(), win_x + 180, win_y + 120, COLOR_BLUE);
        gfx_draw_rect(win_x + 50, win_y + 160, 150, 40, music_is_playing() ? COLOR_RED : COLOR_GREEN);
        gfx_draw_string(music_is_playing() ? "PAUSAR" : "PLAY", win_x + 95, win_y + 175, COLOR_WHITE);
        gfx_draw_rect(win_x + 220, win_y + 160, 180, 40, COLOR_BLUE);
        gfx_draw_string("MUDAR FAIXA", win_x + 250, win_y + 175, COLOR_WHITE);
    } else if (w->app_type == 5) {
        gfx_draw_string("GERENCIADOR DE TAREFAS (ROUND-ROBIN MULTITASKING):", win_x + 30, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 70, win_w - 60, 310, COLOR_NAVY);
        gfx_draw_string("PID   NOME              PRIORIDADE   ESTADO      TICKS", win_x + 40, win_y + 85, COLOR_BLUE);

        int num_t = get_num_tasks();
        for (int i = 0; i < num_t; i++) {
            task_t* t = get_task(i);
            int row_y = win_y + 115 + (i * 25);
            gfx_draw_number(t->pid, win_x + 40, row_y, COLOR_WHITE);
            gfx_draw_string(t->name, win_x + 90, row_y, COLOR_WHITE);

            if (t->priority == 1) gfx_draw_string("1 (ALTA)", win_x + 260, row_y, COLOR_RED);
            else if (t->priority == 2) gfx_draw_string("2 (MEDIA)", win_x + 260, row_y, COLOR_BLUE);
            else gfx_draw_string("3 (BAIXA)", win_x + 260, row_y, COLOR_LIGHT_GRAY);

            if (t->state == TASK_STATE_RUNNING) gfx_draw_string("RODANDO", win_x + 380, row_y, COLOR_GREEN);
            else gfx_draw_string("PRONTO", win_x + 380, row_y, COLOR_WHITE);

            gfx_draw_number(t->time_ticks, win_x + 480, row_y, COLOR_WHITE);
        }
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
        gfx_draw_string("JOGO SNAKE EM RING 3 (SYSCALLS):", win_x + 30, win_y + 45, COLOR_GREEN);
        int canvas_x = win_x + 30, canvas_y = win_y + 70, canvas_w = 460, canvas_h = 300;
        gfx_draw_rect(canvas_x, canvas_y, canvas_w, canvas_h, COLOR_NAVY);

        gfx_draw_rect(food_x, food_y, 10, 10, COLOR_RED);
        gfx_draw_rect(snake_x, snake_y, 12, 12, COLOR_GREEN);
    }
}

void draw_desktop_icon(int x, int y, int app_id, const char* title, const char* sub, uint32_t color) {
    (void)app_id;
    int is_hovered = (mouse_x >= x && mouse_x <= x + 115 && mouse_y >= y && mouse_y <= y + 50);
    uint32_t bg_color = is_hovered ? COLOR_BLUE : COLOR_NAVY;
    uint32_t border_color = is_hovered ? COLOR_GREEN : COLOR_GRAY;

    // Fundo do Ícone com Alpha Transparência (90% Opacidade)
    gfx_draw_rect_alpha(x, y, 115, 50, bg_color, 220);
    gfx_draw_rect(x + 3, y + 3, 109, 14, border_color);
    gfx_draw_string(title, x + 10, y + 6, COLOR_WHITE);
    gfx_draw_string(sub, x + 10, y + 28, color);
}

void ui_render(void) {
    // 1. Renderiza o Plano de Fundo HD (1024x768)
    if (current_wallpaper == 0) gfx_draw_landscape_sunset(0, 0, 1024, 768);
    else if (current_wallpaper == 1) gfx_draw_landscape_cosmos(0, 0, 1024, 768);
    else if (current_wallpaper == 2) gfx_draw_landscape_synthwave(0, 0, 1024, 768);
    else gfx_clear(COLOR_DARK_SLATE);

    // 2. Ícones do Desktop
    int ic_x = 15;
    draw_desktop_icon(ic_x, 20,  0, "1. SHELL",    "DISCO VFS", COLOR_GREEN);
    draw_desktop_icon(ic_x, 80,  1, "2. MEMORIA",  "RAM HEAP", COLOR_GREEN);
    draw_desktop_icon(ic_x, 140, 2, "3. REDE/PING", "RTL8139",   COLOR_GREEN);
    draw_desktop_icon(ic_x, 200, 3, "4. GALERIA",  "PINTOR/BMP",COLOR_GREEN);
    draw_desktop_icon(ic_x, 260, 4, "5. MUSICA",   "CHIPTUNE",  COLOR_GREEN);
    draw_desktop_icon(ic_x, 320, 5, "6. TAREFAS",  "MULTITASK", COLOR_WHITE);
    draw_desktop_icon(ic_x, 380, 6, "7. PAINT",    "BMP ESTUDIO",COLOR_WHITE);
    draw_desktop_icon(ic_x, 440, 7, "8. EXPLORAR", "SINTAXE #|",COLOR_YELLOW);

    // 3. BARRA DE TAREFAS TRANSLÚCIDA (85% OPACIDADE EFEITO VIDRO) NA PARTE INFERIOR HD (Y = 728)
    gfx_draw_rect_alpha(0, 728, 1024, 40, COLOR_NAVY, 215);
    gfx_draw_rect(10, 733, 80, 30, start_menu_open ? COLOR_GREEN : COLOR_BLUE);
    gfx_draw_string("START", 30, 744, COLOR_NAVY);

    rtc_time_t clock_brt;
    rtc_get_time_brt(&clock_brt);
    char time_str[16];
    format_time_string(time_str, clock_brt.hour, clock_brt.minute, clock_brt.second);
    gfx_draw_string(time_str, 900, 744, COLOR_WHITE);

    // 4. Renderiza Janelas Abertas
    for (int z = 1; z <= top_z_index; z++) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z_index == z) draw_single_window(&windows[i]);
        }
    }

    // 5. Menu Start Pop-up
    if (start_menu_open) {
        gfx_draw_rect_alpha(10, 440, 200, 280, COLOR_NAVY, 230);
        gfx_draw_rect(10, 440, 200, 25, COLOR_BLUE);
        gfx_draw_string("MENU START", 20, 448, COLOR_WHITE);

        gfx_draw_string("> 1. SHELL VFS", 20, 475, COLOR_WHITE);
        gfx_draw_string("> 2. MEMORIA (RAM)", 20, 510, COLOR_WHITE);
        gfx_draw_string("> 3. IDT / HARDWARE", 20, 545, COLOR_WHITE);
        gfx_draw_string("> 4. GALERIA (PINTOR)", 20, 580, COLOR_WHITE);
        gfx_draw_string("> 5. PLAYER DE MUSICA", 20, 615, COLOR_WHITE);
        gfx_draw_string("> 6. TAREFAS (CPU)", 20, 650, COLOR_WHITE);
        gfx_draw_string("> 7. PAINT STUDIO", 20, 685, COLOR_WHITE);
    }

    // 6. Ponteiro do Mouse com Sombra
    gfx_draw_cursor(mouse_x, mouse_y);

    gfx_swap_buffers();
}
