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
#include "../include/bgif.h"

#define MAX_WINDOWS 13

typedef struct {
    int id; int app_type; int x, y, w, h; int is_open; int z_index; uint32_t* cache; 
} window_t;

static window_t windows[MAX_WINDOWS];
static int top_z_index = 0, dragging_window_id = -1, drag_off_x = 0, drag_off_y = 0;

static int current_wallpaper = -1;
static int bgif_wallpaper_frame = 0;

static int start_menu_open = 0, gallery_photo = 0;


static char input_buffer[32]; static int input_index = 0;
static char shell_output[128] = "SISTEMA VFS & PLAYER DE VIDEO BGIF PRONTOS!";
static uint32_t paint_canvas[160 * 120]; static uint32_t paint_color = 0xFFFFFF;
static int prev_mouse_left = 0;
static int prev_mouse_x = -1, prev_mouse_y = -1;

static int snake_x = 100, snake_y = 100;
static int snake_dir_x = 5, snake_dir_y = 0;
static int food_x = 200, food_y = 150;

static int video_playing = 1;
static int video_frame = 0;

static int ui_needs_redraw = 1;

static char calc_display[32] = "0";
static long long calc_val1 = 0;
static int calc_op = 0;
static int calc_clear_next = 0;

// === VARIAVEIS DO CAPIVARAPAD (BLOCO DE NOTAS) ===
static char pad_buffer[256] = "BEM-VINDO AO CAPIVARAPAD! DIGITE SEU TEXTO AQUI...";
static int pad_index = 50;

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}

void sys_shutdown(void) {
    serial_write("[POWER] Desligando CapivaraOS...\n");
    sound_play(1046); for (volatile int i = 0; i < 150000; i++);
    sound_play(784);  for (volatile int i = 0; i < 150000; i++);
    sound_play(659);  for (volatile int i = 0; i < 150000; i++);
    sound_play(523);  for (volatile int i = 0; i < 250000; i++);
    sound_stop();

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x600, 0x3400);

    while (1) { asm volatile ("hlt"); }
}

void sys_reboot(void) {
    serial_write("[POWER] Reiniciando CapivaraOS...\n");
    sound_play(880); for (volatile int i = 0; i < 100000; i++);
    sound_stop();

    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);

    while (1) { asm volatile ("hlt"); }
}

static long long parse_calc_display(void) {
    long long val = 0; int is_neg = 0, start = 0;
    if (calc_display[0] == '-') { is_neg = 1; start = 1; }
    for(int i = start; calc_display[i]; i++) {
        val = val * 10 + (calc_display[i] - '0');
    }
    return is_neg ? -val : val;
}

static void format_time_string(char* buf, uint8_t h, uint8_t m, uint8_t s) {
    buf[0] = '0' + (h / 10); buf[1] = '0' + (h % 10); buf[2] = ':';
    buf[3] = '0' + (m / 10); buf[4] = '0' + (m % 10); buf[5] = ':';
    buf[6] = '0' + (s / 10); buf[7] = '0' + (s % 10); buf[8] = ' ';
    buf[9] = 'B'; buf[10] = 'R'; buf[11] = 'T'; buf[12] = '\0';
}

void bring_to_front(int win_id) {
    top_z_index++; windows[win_id].z_index = top_z_index;
    
    windows[win_id].is_open = 1;
    ui_needs_redraw = 1;
}

void ui_init(void) {
    music_init(); net_init();
    kmemset(paint_canvas, 0, sizeof(paint_canvas));
    input_buffer[0] = '\0';

    windows[0] = (window_t){0, 0, 240, 60,  640, 520, 0, 1, NULL};
    windows[1] = (window_t){1, 1, 200, 80,  640, 520, 0, 2, NULL};
    windows[2] = (window_t){2, 2, 160, 100, 640, 520, 0, 3, NULL};
    windows[3] = (window_t){3, 3, 180, 50,  640, 540, 0, 4, NULL};
    windows[4] = (window_t){4, 4, 260, 90,  580, 480, 0, 5, NULL};
    windows[5] = (window_t){5, 5, 200, 60,  660, 500, 0, 6, NULL};
    windows[6] = (window_t){6, 6, 130, 70,  600, 520, 0, 7, NULL};
    windows[7] = (window_t){7, 7, 240, 100, 560, 420, 0, 8, NULL};
    windows[8] = (window_t){8, 8, 150, 70,  580, 480, 0, 9, NULL};
    windows[9] = (window_t){9, 9, 210, 80,  580, 500, 0, 10, NULL};
    windows[10]= (window_t){10, 10, 300, 150, 200, 260, 0, 11, NULL};
    windows[11]= (window_t){11, 11, 220, 100, 600, 480, 0, 12, NULL};
    windows[12]= (window_t){12, 12, 180, 70,  620, 480, 0, 13, NULL};

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t* w = &windows[i];
        w->cache = kmalloc(w->w * w->h * sizeof(uint32_t));
        gfx_set_target(w->cache, w->w, w->h);
        gfx_draw_rect(0, 0, w->w, w->h, COLOR_GRAY);
        gfx_draw_rect(0, 0, w->w, 30, COLOR_LIGHT_GRAY);
        const char* title = "JANELA";
        if (w->app_type == 0) title = "JANELA: TERMINAL SHELL VFS";
        else if (w->app_type == 1) title = "JANELA: GERENCIADOR DE RAM";
        else if (w->app_type == 2) title = "JANELA: DIAGNOSTICO REDE";
        else if (w->app_type == 3) title = "JANELA: GALERIA & BMP";
        else if (w->app_type == 4) title = "JANELA: PLAYER CHIPTUNE";
        else if (w->app_type == 5) title = "JANELA: TAREFAS DA CPU";
        else if (w->app_type == 6) title = "JANELA: PAINT STUDIO";
        else if (w->app_type == 7) title = "JANELA: EXPLORADOR VFS";
        else if (w->app_type == 8) title = "JANELA: JOGO SNAKE";
        else if (w->app_type == 9) title = "JANELA: PLAYER DE VIDEO";
        else if (w->app_type == 10) title = "JANELA: CALCULADORA";
        else if (w->app_type == 11) title = "JANELA: LOGS & UPDATES";
        else if (w->app_type == 12) title = "JANELA: CAPIVARAPAD";
        gfx_draw_string(title, 15, 11, COLOR_WHITE);
        gfx_draw_rect(w->w - 25, 7, 16, 16, COLOR_RED);
    }
    gfx_set_target(NULL, 0, 0);
    windows[9].is_open = 1;

    top_z_index = 13;
    ui_needs_redraw = 1;
}

static void process_shell_command(void) {
    for (int i = 0; i < 128; i++) shell_output[i] = '\0';

    if (kstrcmp(input_buffer, "help") == 0) {
        const char* msg = "COMANDOS: LS, CAT <ARQ>, WRITE <ARQ> <TEXTO>, RUN <ELF>, UDP <MSG>";
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
    } else if (kstrncmp(input_buffer, "run ", 4) == 0) {
        size_t sz = 0;
        const uint8_t* bytes = vfs_read(input_buffer + 4, &sz);
        if (bytes && elf_load_and_run(bytes, sz, input_buffer + 4)) {
            kstrcpy(shell_output, "PROCESSO RING 3 EXECUTADO EM SEGUNDO PLANO!");
        } else {
            kstrcpy(shell_output, "ERRO AO CARREGAR EXECUTAVEL ELF!");
        }
    } else if (kstrncmp(input_buffer, "udp ", 4) == 0) {
        uint32_t target_ip = ((uint32_t)(10) | ((uint32_t)(0) << 8) | ((uint32_t)(2) << 16) | ((uint32_t)(2) << 24));
        net_send_udp(target_ip, 12345, 8888, input_buffer + 4, kstrlen(input_buffer + 4));
        kstrcpy(shell_output, "PACOTE UDP ENVIADO PARA 10.0.2.2:8888!");
    } else if (kstrcmp(input_buffer, "clear") == 0) {
        shell_output[0] = '\0';
    }
    input_index = 0; input_buffer[0] = '\0';
    ui_needs_redraw = 1;
}

void ui_handle_keyboard(void) {
    if (last_key_pressed != 0) {
        char c = last_key_pressed; last_key_pressed = 0;
        ui_needs_redraw = 1;

        // SE A JANELA DO BLOCO DE NOTAS ESTIVER EM FOCO (Z-INDEX MAIS ALTO)
        int top_win = -1, highest_z = -1;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z_index > highest_z) {
                highest_z = windows[i].z_index;
                top_win = i;
            }
        }

        if (top_win != -1 && windows[top_win].app_type == 12) { // CAPIVARAPAD
            if (c == '\b') {
                if (pad_index > 0) pad_buffer[--pad_index] = '\0';
            } else if (c == '\n') {
                if (pad_index < 240) { pad_buffer[pad_index++] = ' '; pad_buffer[pad_index] = '\0'; }
            } else if (pad_index < 240) {
                pad_buffer[pad_index++] = c;
                pad_buffer[pad_index] = '\0';
            }
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

    if (mouse_x != prev_mouse_x || mouse_y != prev_mouse_y || just_pressed) {
        ui_needs_redraw = 1;
        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;
    }

    if (just_pressed) sound_click();
    int screen_h = gfx_get_height();

    if (start_menu_open && just_pressed && mouse_x >= 10 && mouse_x <= 200) {
        int btn_power_y = screen_h - 40;
        if (mouse_y >= btn_power_y - 25 && mouse_y <= btn_power_y) {
            if (mouse_x <= 100) { sys_shutdown(); return; }
            else { sys_reboot(); return; }
        }
    }

    if (just_pressed && mouse_x >= 15 && mouse_x <= 135) {
        if (mouse_y >= 20 && mouse_y <= 75) bring_to_front(0);
        else if (mouse_y >= 80 && mouse_y <= 135) bring_to_front(1);
        else if (mouse_y >= 140 && mouse_y <= 195) bring_to_front(2);
        else if (mouse_y >= 200 && mouse_y <= 255) bring_to_front(3);
        else if (mouse_y >= 260 && mouse_y <= 315) bring_to_front(4);
        else if (mouse_y >= 320 && mouse_y <= 375) bring_to_front(5);
        else if (mouse_y >= 380 && mouse_y <= 435) bring_to_front(6);
        else if (mouse_y >= 440 && mouse_y <= 495) bring_to_front(7);
        else if (mouse_y >= 500 && mouse_y <= 555) bring_to_front(9);
    }
    
    if (just_pressed && mouse_x >= 140 && mouse_x <= 255) {
        if (mouse_y >= 20 && mouse_y <= 75) bring_to_front(10);
        else if (mouse_y >= 80 && mouse_y <= 135) bring_to_front(11);
        else if (mouse_y >= 140 && mouse_y <= 195) bring_to_front(12); // CAPIVARAPAD
    }

    if (just_pressed && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= screen_h - 35 && mouse_y <= screen_h - 5) {
        start_menu_open = !start_menu_open; ui_needs_redraw = 1; return;
    }

    if (start_menu_open && just_pressed && mouse_x >= 10 && mouse_x <= 200 && mouse_y >= screen_h - 480 && mouse_y <= screen_h - 70) {
        for (int i = 0; i < 12; i++) {
            int my = screen_h - 445 + (i * 30);
            if (mouse_y >= my - 5 && mouse_y <= my + 25) {
                int mapping[] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12};
                bring_to_front(mapping[i]);
            }
        }
        start_menu_open = 0; ui_needs_redraw = 1; return;
    }

    if (start_menu_open && just_pressed) { start_menu_open = 0; ui_needs_redraw = 1; }

    if (just_pressed) {
        int clicked_win = find_clicked_window(mouse_x, mouse_y);
        if (clicked_win != -1) {
            bring_to_front(clicked_win);
            window_t* w = &windows[clicked_win];

            if (mouse_x >= (w->x + w->w - 25) && mouse_x <= (w->x + w->w - 9) &&
                mouse_y >= (w->y + 7) && mouse_y <= (w->y + 23)) {
                w->is_open = 0; dragging_window_id = -1; ui_needs_redraw = 1; return;
            }

            if (mouse_y >= w->y && mouse_y <= (w->y + 30)) {
                dragging_window_id = clicked_win; drag_off_x = mouse_x - w->x; drag_off_y = mouse_y - w->y;
            }

            // BOTÕES DO CAPIVARAPAD (SALVAR E LIMPAR)
            if (w->app_type == 12 && mouse_y >= w->y + 420 && mouse_y <= w->y + 455) {
                if (mouse_x >= w->x + 30 && mouse_x <= w->x + 220) { // SALVAR .TXT
                    vfs_write_file("nota.txt", (const uint8_t*)pad_buffer, kstrlen(pad_buffer));
                    sound_click();
                } else if (mouse_x >= w->x + 240 && mouse_x <= w->x + 400) { // LIMPAR
                    pad_buffer[0] = '\0'; pad_index = 0;
                    sound_click();
                }
                ui_needs_redraw = 1;
            }

            if (w->app_type == 9 && mouse_y >= w->y + 400 && mouse_y <= w->y + 440) {
                if (mouse_x >= w->x + 30 && mouse_x <= w->x + 180) video_playing = !video_playing;
                else if (mouse_x >= w->x + 200 && mouse_x <= w->x + 360) video_frame++;
                else if (mouse_x >= w->x + 380 && mouse_x <= w->x + 550) current_wallpaper = 4;
                ui_needs_redraw = 1;
            }

            if (w->app_type == 4 && mouse_y >= w->y + 160 && mouse_y <= w->y + 200) {
                if (mouse_x >= w->x + 50 && mouse_x <= w->x + 200) music_toggle_play();
                else if (mouse_x >= w->x + 220 && mouse_x <= w->x + 400) music_next_track();
                ui_needs_redraw = 1;
            }
            
            if (w->app_type == 10) {
                int col = (mouse_x - (w->x + 15)) / 43;
                int row = (mouse_y - (w->y + 100)) / 35;
                if (col >= 0 && col < 4 && row >= 0 && row < 4) {
                    const char* btns[16] = { "7", "8", "9", "/", "4", "5", "6", "*", "1", "2", "3", "-", "C", "0", "=", "+" };
                    const char* b = btns[row*4 + col];
                    
                    if (b[0] >= '0' && b[0] <= '9') {
                        if (calc_clear_next || (calc_display[0] == '0' && calc_display[1] == '\0')) {
                            calc_display[0] = b[0]; calc_display[1] = '\0';
                            calc_clear_next = 0;
                        } else {
                            int len = kstrlen(calc_display);
                            if (len < 15) { calc_display[len] = b[0]; calc_display[len+1] = '\0'; }
                        }
                    } else if (b[0] == 'C') {
                        calc_display[0] = '0'; calc_display[1] = '\0';
                        calc_val1 = 0; calc_op = 0; calc_clear_next = 0;
                    } else if (b[0] == '=') {
                        long long val2 = parse_calc_display();
                        long long res = 0;
                        if (calc_op == 1) res = calc_val1 + val2;
                        else if (calc_op == 2) res = calc_val1 - val2;
                        else if (calc_op == 3) res = calc_val1 * val2;
                        else if (calc_op == 4 && val2 != 0) res = calc_val1 / val2;
                        else res = val2;
                        
                        if (res == 0) { calc_display[0] = '0'; calc_display[1] = '\0'; }
                        else {
                            char rev[32]; int idx = 0;
                            long long temp = res < 0 ? -res : res;
                            while(temp > 0) { rev[idx++] = '0' + (temp % 10); temp /= 10; }
                            if (res < 0) rev[idx++] = '-';
                            for(int i=0; i<idx; i++) calc_display[i] = rev[idx - 1 - i];
                            calc_display[idx] = '\0';
                        }
                        calc_op = 0; calc_clear_next = 1;
                    } else {
                        calc_val1 = parse_calc_display();
                        if (b[0] == '+') calc_op = 1;
                        if (b[0] == '-') calc_op = 2;
                        if (b[0] == '*') calc_op = 3;
                        if (b[0] == '/') calc_op = 4;
                        calc_clear_next = 1;
                    }
                    ui_needs_redraw = 1;
                }
            }

            if (w->app_type == 3) {
                int btn_y1 = w->y + 390; int btn_y2 = w->y + 430;
                if (mouse_y >= btn_y1 && mouse_y <= btn_y1 + 30) {
                    if (mouse_x >= w->x + 20 && mouse_x <= w->x + 110) gallery_photo = 0;
                    else if (mouse_x >= w->x + 120 && mouse_x <= w->x + 200) gallery_photo = 1;
                    else if (mouse_x >= w->x + 210 && mouse_x <= w->x + 310) gallery_photo = 2;
                    else if (mouse_x >= w->x + 320 && mouse_x <= w->x + 450) gallery_photo = 3;
                }

                if (mouse_y >= btn_y2 && mouse_y <= btn_y2 + 30) {
                    if (mouse_x >= w->x + 20 && mouse_x <= w->x + 210) current_wallpaper = gallery_photo;
                    else if (mouse_x >= w->x + 225 && mouse_x <= w->x + 435) vfs_write_file("paisagem.art", (const uint8_t*)"ARTE RECENTE DA GALERIA", 23);
                    else if (mouse_x >= w->x + 450 && mouse_x <= w->x + 610) current_wallpaper = -1;
                }
                ui_needs_redraw = 1;
            }

            if (w->app_type == 6 && mouse_y >= w->y + 430 && mouse_y <= w->y + 460) {
                if (mouse_x >= w->x + 20 && mouse_x <= w->x + 60) paint_color = 0xFF0000;
                else if (mouse_x >= w->x + 70 && mouse_x <= w->x + 110) paint_color = 0x00FF00;
                else if (mouse_x >= w->x + 120 && mouse_x <= w->x + 160) paint_color = 0x0000FF;
                else if (mouse_x >= w->x + 170 && mouse_x <= w->x + 210) paint_color = 0xFFFFFF;
                else if (mouse_x >= w->x + 220 && mouse_x <= w->x + 260) paint_color = 0x000000;
                else if (mouse_x >= w->x + 310 && mouse_x <= w->x + 510) bmp_export("arte.bmp", paint_canvas, 160, 120);
                ui_needs_redraw = 1;
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
                ui_needs_redraw = 1;
            }
        }
    }

    if (click && dragging_window_id != -1) {
        window_t* w = &windows[dragging_window_id];
        w->x = mouse_x - drag_off_x; w->y = mouse_y - drag_off_y;
        if (w->x < 0) w->x = 0;
        if (w->x + w->w > 1024) w->x = 1024 - w->w;
        if (w->y < 0) w->y = 0;
        if (w->y + w->h > 728) w->y = 728 - w->h;
        ui_needs_redraw = 1;
    }

    if (!click) dragging_window_id = -1;
}

void draw_single_window(window_t* w) {
    if (!w->is_open) return;
    int win_x = w->x; int win_y = w->y; int win_w = w->w; int win_h = w->h;
    gfx_draw_rect_alpha(win_x + 8, win_y + 8, win_w, win_h, 0x000000, 100);
    gfx_blit(w->cache, win_x, win_y, win_w, win_h);


    if (w->app_type == 0) {
        gfx_draw_string("TERMINAL SHELL VFS:", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 105, win_w - 60, 35, COLOR_NAVY);
        gfx_draw_string("capivaraos> ", win_x + 40, win_y + 118, COLOR_GREEN);
        gfx_draw_string(input_buffer, win_x + 130, win_y + 118, COLOR_WHITE);
        gfx_draw_rect(win_x + 30, win_y + 160, win_w - 60, 240, COLOR_NAVY);
        gfx_draw_string(shell_output, win_x + 50, win_y + 220, COLOR_WHITE);
    } else if (w->app_type == 1) {
        gfx_draw_string("GERENCIADOR DE MEMORIA HEAP (RAM):", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 300, COLOR_NAVY);
        gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 120, COLOR_WHITE);
        gfx_draw_number_64(memory_get_total_allocated(), win_x + 220, win_y + 120, COLOR_GREEN);
        gfx_draw_string(" BYTES", win_x + 290, win_y + 120, COLOR_WHITE);
    } else if (w->app_type == 2) {
        gfx_draw_string("DIAGNOSTICO DA PLACA DE REDE REALTEK RTL8139:", win_x + 30, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 70, win_w - 60, 350, COLOR_NAVY);
        gfx_draw_string("STATUS DA PLACA: REALTEK RTL8139 (PCI) - ONLINE", win_x + 50, win_y + 100, COLOR_GREEN);
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
    } else if (w->app_type == 4) {
        gfx_draw_string("PLAYER DE MUSICA CHIPTUNE 8-BIT:", win_x + 30, win_y + 50, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 320, COLOR_NAVY);
        gfx_draw_string(music_get_track_name(), win_x + 180, win_y + 120, COLOR_BLUE);
    } else if (w->app_type == 5) {
        gfx_draw_string("GERENCIADOR DE TAREFAS (ROUND-ROBIN MULTITASKING):", win_x + 30, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 30, win_y + 70, win_w - 60, 310, COLOR_NAVY);
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
        gfx_draw_string("EXPLORADOR DE ARQUIVOS (NOTACAO #|):", win_x + 30, win_y + 45, COLOR_GREEN);
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
    } else if (w->app_type == 9) {
        gfx_draw_string("REPRODUTOR DE VIDEO BGIF (.BMP-GIF 24 FPS):", win_x + 30, win_y + 45, COLOR_GREEN);
        int canvas_x = win_x + 30, canvas_y = win_y + 70, canvas_w = 500, canvas_h = 310;
        size_t bgif_size = 0;
        const uint8_t* bgif_bytes = vfs_read("animacao.bgif", &bgif_size);
        if (bgif_bytes) {
            static uint32_t bgif_fps_ticks = 0;
            if (video_playing) {
                bgif_fps_ticks++;
                if (bgif_fps_ticks % 4 == 0) video_frame++;
            }
            bgif_draw_frame(bgif_bytes, bgif_size, video_frame, canvas_x, canvas_y, canvas_w, canvas_h);
            int btn_y = win_y + 400;
            gfx_draw_rect(win_x + 30, btn_y, 140, 35, video_playing ? COLOR_RED : COLOR_GREEN);
            gfx_draw_string(video_playing ? "PAUSAR" : "PLAY VIDEO", win_x + 45, btn_y + 12, COLOR_WHITE);
            gfx_draw_rect(win_x + 180, btn_y, 170, 35, COLOR_BLUE);
            gfx_draw_string("PROXIMO QUADRO", win_x + 195, btn_y + 12, COLOR_WHITE);
            gfx_draw_rect(win_x + 360, btn_y, 190, 35, COLOR_PURPLE);
            gfx_draw_string("BGIF COMO WALLPAPER", win_x + 370, btn_y + 12, COLOR_WHITE);
        }
    } else if (w->app_type == 10) {
        gfx_draw_string("CALCULADORA:", win_x + 15, win_y + 35, COLOR_GREEN);
        gfx_draw_rect(win_x + 15, win_y + 55, win_w - 30, 30, COLOR_NAVY);
        gfx_draw_string(calc_display, win_x + 25, win_y + 65, COLOR_WHITE);
        
        const char* btns[16] = {
            "7", "8", "9", "/",
            "4", "5", "6", "*",
            "1", "2", "3", "-",
            "C", "0", "=", "+"
        };
        for(int row=0; row<4; row++) {
            for(int col=0; col<4; col++) {
                int bx = win_x + 15 + col*43;
                int by = win_y + 95 + row*35;
                gfx_draw_rect(bx, by, 38, 30, COLOR_BLUE);
                gfx_draw_string(btns[row*4 + col], bx + 16, by + 11, COLOR_WHITE);
            }
        }
    } else if (w->app_type == 11) {
        gfx_draw_string("LOGS DE ATUALIZACAO - CAPIVARAOS v1.4:", win_x + 20, win_y + 45, COLOR_GREEN);
        gfx_draw_rect(win_x + 20, win_y + 70, win_w - 40, 380, COLOR_NAVY);

        gfx_draw_string("VERSAO ATUAL: CapivaraOS 64-bit v1.4", win_x + 35, win_y + 90, COLOR_YELLOW);
        gfx_draw_string("--------------------------------------------", win_x + 35, win_y + 110, COLOR_GRAY);

        gfx_draw_string("[+] ADICIONADO: App 12 CapivaraPad (Bloco de Notas)", win_x + 35, win_y + 130, COLOR_GREEN);
        gfx_draw_string("[+] ADICIONADO: Suporte a Letras Acentuadas e Cedilha", win_x + 35, win_y + 150, COLOR_GREEN);
        gfx_draw_string("[+] ADICIONADO: Suporte a Teclas SHIFT, Caps e Simbolos", win_x + 35, win_y + 170, COLOR_GREEN);

        gfx_draw_string("[*] OTIMIZADO: Teclado e Mouse sem Latencia (Buffer PS/2)", win_x + 35, win_y + 200, COLOR_BLUE);
        gfx_draw_string("[*] OTIMIZADO: Efeitos Sonoros com HLT sem gastar CPU", win_x + 35, win_y + 220, COLOR_BLUE);

        gfx_draw_string("[-] REMOVIDO: Laços de Espera Cega (for/while)", win_x + 35, win_y + 250, COLOR_RED);
    } else if (w->app_type == 12) { // ================= CAPIVARAPAD =================
        gfx_draw_string("CAPIVARAPAD - BLOCO DE NOTAS DIGITAVEL:", win_x + 20, win_y + 45, COLOR_GREEN);
        
        // FOLHA DE PAPEL DIGITAL BRANCA PARA TEXTO
        gfx_draw_rect(win_x + 20, win_y + 70, win_w - 40, 330, COLOR_WHITE);
        
        // EXIBE O TEXTO DIGITADO PELO USUARIO
        gfx_draw_string(pad_buffer, win_x + 30, win_y + 85, COLOR_DARK_SLATE);
        
        // CURSOR DE DIGITACAO PISCANTE
        int cursor_x_pos = win_x + 30 + ((pad_index % 70) * 8);
        int cursor_y_pos = win_y + 85 + ((pad_index / 70) * 12);
        gfx_draw_rect(cursor_x_pos, cursor_y_pos, 2, 10, COLOR_RED);

        // BOTOES DE AÇÃO
        int btn_pad_y = win_y + 420;
        gfx_draw_rect(win_x + 30, btn_pad_y, 190, 35, COLOR_BLUE);
        gfx_draw_string("SALVAR EM NOTA.TXT", win_x + 40, btn_pad_y + 12, COLOR_WHITE);

        gfx_draw_rect(win_x + 240, btn_pad_y, 160, 35, COLOR_GRAY);
        gfx_draw_string("NOVO / LIMPAR", win_x + 260, btn_pad_y + 12, COLOR_WHITE);
    }
}

void draw_desktop_icon(int x, int y, int app_id, const char* title, const char* sub, uint32_t color) {
    (void)app_id;
    int is_hovered = (mouse_x >= x && mouse_x <= x + 115 && mouse_y >= y && mouse_y <= y + 50);
    uint32_t bg_color = is_hovered ? COLOR_BLUE : COLOR_NAVY;
    uint32_t border_color = is_hovered ? COLOR_GREEN : COLOR_GRAY;

    gfx_draw_rect_alpha(x, y, 115, 50, bg_color, 210);
    gfx_draw_rect(x + 3, y + 3, 109, 14, border_color);
    gfx_draw_string(title, x + 10, y + 6, COLOR_WHITE);
    gfx_draw_string(sub, x + 10, y + 28, color);
}

void ui_render(void) {
    if (video_playing || current_wallpaper == 4) {
        ui_needs_redraw = 1;
    }

    rtc_time_t clock_brt;
    rtc_get_time_brt(&clock_brt);
    static uint8_t last_sec = 255;
    if (clock_brt.second != last_sec) {
        last_sec = clock_brt.second;
        ui_needs_redraw = 1;
    }

    if (!ui_needs_redraw) return;
    ui_needs_redraw = 0;

    int screen_w = gfx_get_width();
    int screen_h = gfx_get_height();

    if (current_wallpaper == 4) {
        size_t bgif_sz = 0;
        const uint8_t* bgif_bytes = vfs_read("animacao.bgif", &bgif_sz);
        if (bgif_bytes) {
            bgif_wallpaper_frame++;
            bgif_draw_frame(bgif_bytes, bgif_sz, bgif_wallpaper_frame, 0, 0, screen_w, screen_h);
        } else {
            gfx_clear(COLOR_DARK_SLATE);
        }
    } else if (current_wallpaper == 3) {
        size_t bmp_sz = 0;
        const uint8_t* bmp_bytes = vfs_read("foto.bmp", &bmp_sz);
        if (bmp_bytes) {
            bmp_draw(bmp_bytes, 0, 0, screen_w, screen_h);
        } else {
            gfx_clear(COLOR_DARK_SLATE);
        }
    } else if (current_wallpaper == 0) {
        gfx_draw_landscape_sunset(0, 0, screen_w, screen_h);
    } else if (current_wallpaper == 1) {
        gfx_draw_landscape_cosmos(0, 0, screen_w, screen_h);
    } else if (current_wallpaper == 2) {
        gfx_draw_landscape_synthwave(0, 0, screen_w, screen_h);
    } else {
        gfx_clear(COLOR_DARK_SLATE);
    }

    int ic_x = 15;
    draw_desktop_icon(ic_x, 20,  0, "1. SHELL",    "DISCO VFS", COLOR_GREEN);
    draw_desktop_icon(ic_x, 80,  1, "2. MEMORIA",  "RAM HEAP", COLOR_GREEN);
    draw_desktop_icon(ic_x, 140, 2, "3. REDE/PING", "RTL8139",   COLOR_GREEN);
    draw_desktop_icon(ic_x, 200, 3, "4. GALERIA",  "PINTOR/BMP",COLOR_GREEN);
    draw_desktop_icon(ic_x, 260, 4, "5. MUSICA",   "CHIPTUNE",  COLOR_GREEN);
    draw_desktop_icon(ic_x, 320, 5, "6. TAREFAS",  "MULTITASK", COLOR_WHITE);
    draw_desktop_icon(ic_x, 380, 6, "7. PAINT",    "BMP ESTUDIO",COLOR_WHITE);
    draw_desktop_icon(ic_x, 440, 7, "8. EXPLORAR", "SINTAXE #|",COLOR_YELLOW);
    draw_desktop_icon(ic_x, 500, 9, "9. VIDEO",    "BGIF PLAYER",COLOR_PURPLE);
    
    // FILEIRA 2: CALCULADORA, LOGS E CAPIVARAPAD
    draw_desktop_icon(140, 20,  10, "10. CALC", "CALCULADORA", COLOR_ORANGE);
    draw_desktop_icon(140, 80,  11, "11. LOGS", "CHANGELOG",   COLOR_GREEN);
    draw_desktop_icon(140, 140, 12, "12. NOTAS", "CAPIVARAPAD",COLOR_YELLOW);

    gfx_draw_rect_alpha(0, screen_h - 40, screen_w, 40, COLOR_NAVY, 215);
    gfx_draw_rect(10, screen_h - 35, 80, 30, start_menu_open ? COLOR_GREEN : COLOR_BLUE);
    gfx_draw_string("START", 30, screen_h - 24, COLOR_NAVY);

    char time_str[16];
    format_time_string(time_str, clock_brt.hour, clock_brt.minute, clock_brt.second);
    gfx_draw_string(time_str, screen_w - 130, screen_h - 24, COLOR_WHITE);

    for (int z = 1; z <= top_z_index; z++) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z_index == z) draw_single_window(&windows[i]);
        }
    }

    if (start_menu_open) {
        gfx_draw_rect_alpha(10, screen_h - 480, 200, 440, COLOR_NAVY, 230);
        gfx_draw_rect(10, screen_h - 480, 200, 25, COLOR_BLUE);
        gfx_draw_string("MENU START", 20, screen_h - 472, COLOR_WHITE);
        
        const char* menu_items[] = {
            "> 1. SHELL VFS",
            "> 2. MEMORIA (RAM)",
            "> 3. IDT / HARDWARE",
            "> 4. GALERIA (PINTOR)",
            "> 5. PLAYER DE MUSICA",
            "> 6. TAREFAS (CPU)",
            "> 7. PAINT STUDIO",
            "> 8. EXPLORAR",
            "> 9. PLAYER BGIF",
            "> 10. CALCULADORA",
            "> 11. LOGS / UPDATES",
            "> 12. CAPIVARAPAD"
        };
        for (int i = 0; i < 12; i++) {
            gfx_draw_string(menu_items[i], 20, screen_h - 445 + (i * 30), COLOR_WHITE);
        }

        int btn_power_y = screen_h - 40;
        gfx_draw_rect(15, btn_power_y - 25, 85, 25, COLOR_RED);
        gfx_draw_string("DESLIGAR", 22, btn_power_y - 17, COLOR_WHITE);

        gfx_draw_rect(110, btn_power_y - 25, 90, 25, COLOR_ORANGE);
        gfx_draw_string("REINICIAR", 115, btn_power_y - 17, COLOR_WHITE);
    }

    gfx_draw_cursor(mouse_x, mouse_y);
    gfx_swap_buffers();
}
