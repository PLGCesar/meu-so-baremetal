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
    int id; 
    int app_type; 
    int x, y, w, h; 
    int is_open; 
    int z_index; 
    uint32_t* cache; 
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

static char pad_buffer[256] = "BEM-VINDO AO CAPIVARAPAD! DIGITE SEU TEXTO AQUI...";
static int pad_index = 50;

// Mapeamento de Icones de Cada App
static char app_icon_files[13][32] = {
    "shell.bmp", "ram.bmp", "net.bmp", "gallery.bmp", "music.bmp",
    "tasks.bmp", "paint.bmp", "vfs.bmp", "snake.bmp", "video.bmp",
    "calc.bmp", "logs.bmp", "pad.bmp"
};

static int selected_icon_target_app = 0;

static void load_icons_config(void) {
    size_t sz = 0;
    const uint8_t* cfg = vfs_read("icons.cfg", &sz);
    if (cfg && sz > 0) {
        size_t idx = 0;
        for (int app = 0; app < 13 && idx < sz; app++) {
            int pos = 0;
            while (idx < sz && cfg[idx] != '\n' && cfg[idx] != '\r' && pos < 31) {
                app_icon_files[app][pos++] = cfg[idx++];
            }
            app_icon_files[app][pos] = '\0';
            while (idx < sz && (cfg[idx] == '\n' || cfg[idx] == '\r')) idx++;
        }
    }
}

static void save_icons_config(void) {
    char cfg_buf[512];
    cfg_buf[0] = '\0';
    for (int i = 0; i < 13; i++) {
        kstrcpy(cfg_buf + kstrlen(cfg_buf), app_icon_files[i]);
        kstrcpy(cfg_buf + kstrlen(cfg_buf), "\n");
    }
    vfs_write_file("icons.cfg", (const uint8_t*)cfg_buf, kstrlen(cfg_buf));
}

static inline void outw(uint16_t port, uint16_t val) { asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port)); }
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }

void sys_shutdown(void) {
    serial_write("[POWER] Desligando CapivaraOS...\n");
    sound_play(1046); for (volatile int i = 0; i < 150000; i++);
    sound_play(784);  for (volatile int i = 0; i < 150000; i++);
    sound_play(659);  for (volatile int i = 0; i < 150000; i++);
    sound_play(523);  for (volatile int i = 0; i < 250000; i++);
    sound_stop();

    outw(0x604, 0x2000); outw(0xB004, 0x2000); outw(0x600, 0x3400);
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
    for(int i = start; calc_display[i]; i++) { val = val * 10 + (calc_display[i] - '0'); }
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

    load_icons_config();

    windows[0] = (window_t){0, 0, 240, 60,  640, 520, 0, 1, NULL};
    windows[1] = (window_t){1, 1, 200, 80,  640, 520, 0, 2, NULL};
    windows[2] = (window_t){2, 2, 160, 100, 640, 520, 0, 3, NULL};
    windows[3] = (window_t){3, 3, 180, 50,  640, 520, 0, 4, NULL};
    windows[4] = (window_t){4, 4, 260, 90,  580, 480, 0, 5, NULL};
    windows[5] = (window_t){5, 5, 200, 60,  660, 500, 0, 6, NULL};
    windows[6] = (window_t){6, 6, 130, 70,  600, 520, 0, 7, NULL};
    windows[7] = (window_t){7, 7, 240, 100, 560, 420, 0, 8, NULL};
    windows[8] = (window_t){8, 8, 150, 70,  580, 480, 0, 9, NULL};
    windows[9] = (window_t){9, 9, 210, 80,  580, 500, 1, 10, NULL};
    windows[10]= (window_t){10, 10, 300, 150, 200, 260, 0, 11, NULL};
    windows[11]= (window_t){11, 11, 220, 100, 600, 480, 0, 12, NULL};
    windows[12]= (window_t){12, 12, 180, 70,  620, 480, 0, 13, NULL};

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t* w = &windows[i];
        w->cache = (uint32_t*)kmalloc(w->w * w->h * sizeof(uint32_t));
        gfx_set_target(w->cache, w->w, w->h);
        gfx_draw_rect(0, 0, w->w, w->h, COLOR_GRAY);
        gfx_draw_rect(0, 0, w->w, 30, COLOR_LIGHT_GRAY);
        const char* title = "JANELA";
        if (w->app_type == 0) title = "JANELA: TERMINAL SHELL VFS";
        else if (w->app_type == 1) title = "JANELA: GERENCIADOR DE RAM";
        else if (w->app_type == 2) title = "JANELA: DIAGNOSTICO REDE";
        else if (w->app_type == 3) title = "JANELA: GALERIA & ICONFIT";
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
    top_z_index = 13;
    ui_needs_redraw = 1;
}

static void process_shell_command(void) {
    for (int i = 0; i < 128; i++) shell_output[i] = '\0';
    if (kstrcmp(input_buffer, "help") == 0) {
        kstrcpy(shell_output, "COMANDOS: LS, CAT <ARQ>, WRITE <ARQ> <TEXTO>, RUN <ELF>, UDP <MSG>");
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
            kstrcpy(shell_output, "ARQUIVO GRAVADO COM SUCESSO!");
        }
    }
    input_index = 0; input_buffer[0] = '\0';
    ui_needs_redraw = 1;
}

void ui_handle_keyboard(void) {
    if (last_key_pressed != 0) {
        char c = last_key_pressed; last_key_pressed = 0;
        ui_needs_redraw = 1;

        int top_win = -1, highest_z = -1;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z_index > highest_z) {
                highest_z = windows[i].z_index; top_win = i;
            }
        }

        if (top_win != -1 && windows[top_win].app_type == 12) {
            if (c == '\b') { if (pad_index > 0) pad_buffer[--pad_index] = '\0'; }
            else if (pad_index < 240) { pad_buffer[pad_index++] = c; pad_buffer[pad_index] = '\0'; }
            return;
        }

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
        prev_mouse_x = mouse_x; prev_mouse_y = mouse_y;
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

    // Clique nos icones do Desktop (Coluna 1)
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
    // Coluna 2
    if (just_pressed && mouse_x >= 145 && mouse_x <= 265) {
        if (mouse_y >= 20 && mouse_y <= 75) bring_to_front(10);
        else if (mouse_y >= 80 && mouse_y <= 135) bring_to_front(11);
        else if (mouse_y >= 140 && mouse_y <= 195) bring_to_front(12);
    }

    if (just_pressed && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= screen_h - 35 && mouse_y <= screen_h - 5) {
        start_menu_open = !start_menu_open; ui_needs_redraw = 1; return;
    }

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

            // Atribuir novo Ícone do App no App Galeria (App 3)
            if (w->app_type == 3) {
                int btn_y1 = w->y + 390; int btn_y2 = w->y + 430;
                if (mouse_y >= btn_y1 && mouse_y <= btn_y1 + 30) {
                    if (mouse_x >= w->x + 20 && mouse_x <= w->x + 110) gallery_photo = 0;
                    else if (mouse_x >= w->x + 120 && mouse_x <= w->x + 200) gallery_photo = 1;
                    else if (mouse_x >= w->x + 210 && mouse_x <= w->x + 310) gallery_photo = 2;
                    else if (mouse_x >= w->x + 320 && mouse_x <= w->x + 450) gallery_photo = 3;
                }
                if (mouse_y >= btn_y2 && mouse_y <= btn_y2 + 30) {
                    if (mouse_x >= w->x + 20 && mouse_x <= w->x + 180) {
                        selected_icon_target_app = (selected_icon_target_app + 1) % 13;
                    } else if (mouse_x >= w->x + 190 && mouse_x <= w->x + 400) {
                        const char* pic_names[] = {"foto.bmp", "gallery.bmp", "paint.bmp", "foto.bmp"};
                        kstrcpy(app_icon_files[selected_icon_target_app], pic_names[gallery_photo]);
                        save_icons_config();
                    }
                }
                ui_needs_redraw = 1;
            }

            if (w->app_type == 6 && mouse_y >= w->y + 430 && mouse_y <= w->y + 460) {
                if (mouse_x >= w->x + 310 && mouse_x <= w->x + 510) {
                    bmp_export("arte.bmp", paint_canvas, 160, 120);
                    // Define arte.bmp como ícone do app atual
                    kstrcpy(app_icon_files[6], "arte.bmp");
                    save_icons_config();
                }
                ui_needs_redraw = 1;
            }
        }
    }

    if (click && dragging_window_id != -1) {
        window_t* w = &windows[dragging_window_id];
        w->x = mouse_x - drag_off_x; w->y = mouse_y - drag_off_y;
        if (w->x < 0) w->x = 0; if (w->x + w->w > 1024) w->x = 1024 - w->w;
        if (w->y < 0) w->y = 0; if (w->y + w->h > 728) w->y = 728 - w->h;
        ui_needs_redraw = 1;
    }

    if (!click) dragging_window_id = -1;
}

void draw_single_window(window_t* w) {
    if (!w->is_open) return;
    int win_x = w->x; int win_y = w->y; int win_w = w->w; int win_h = w->h;
    gfx_draw_rect_alpha(win_x + 8, win_y + 8, win_w, win_h, 0x000000, 100);
    if(w->cache) { gfx_blit(w->cache, win_x, win_y, win_w, win_h); }

    if (w->app_type == 3) {
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
        gfx_draw_rect(win_x + 20, btn_y2, 160, 30, COLOR_PURPLE);
        gfx_draw_string("MUDAR ALVO APP", win_x + 25, btn_y2 + 11, COLOR_WHITE);

        gfx_draw_rect(win_x + 190, btn_y2, 210, 30, COLOR_GREEN);
        gfx_draw_string("DEFINIR COMO ICONE", win_x + 200, btn_y2 + 11, COLOR_NAVY);
    }
}

// Renderiza Icone Mini BMP Real no Desktop
void draw_desktop_icon(int x, int y, int app_id, const char* title, const char* sub, uint32_t color) {
    int is_hovered = (mouse_x >= x && mouse_x <= x + 120 && mouse_y >= y && mouse_y <= y + 55);
    uint32_t bg_color = is_hovered ? COLOR_BLUE : COLOR_NAVY;
    uint32_t border_color = is_hovered ? COLOR_GREEN : COLOR_GRAY;

    gfx_draw_rect_alpha(x, y, 120, 55, bg_color, 210);
    gfx_draw_rect(x + 2, y + 2, 116, 51, border_color);

    // Desenha o Mini Ícone BMP Real
    size_t sz = 0;
    const uint8_t* icon_data = vfs_read(app_icon_files[app_id], &sz);
    if (icon_data) {
        bmp_draw_icon(icon_data, x + 6, y + 11, 32, 32, 0xFF00FF);
    } else {
        gfx_draw_rect(x + 6, y + 11, 32, 32, color);
    }

    gfx_draw_string(title, x + 42, y + 10, COLOR_WHITE);
    gfx_draw_string(sub, x + 42, y + 28, color);
}

void ui_render(void) {
    if (!ui_needs_redraw) return;
    ui_needs_redraw = 0;

    int screen_w = gfx_get_width();
    int screen_h = gfx_get_height();

    gfx_clear(COLOR_DARK_SLATE);

    int ic_x1 = 15;
    draw_desktop_icon(ic_x1, 20,  0, "1. SHELL",    "DISCO VFS", COLOR_GREEN);
    draw_desktop_icon(ic_x1, 80,  1, "2. MEMORIA",  "RAM HEAP", COLOR_GREEN);
    draw_desktop_icon(ic_x1, 140, 2, "3. REDE/PING", "RTL8139",   COLOR_GREEN);
    draw_desktop_icon(ic_x1, 200, 3, "4. GALERIA",  "ICONFIT",   COLOR_GREEN);
    draw_desktop_icon(ic_x1, 260, 4, "5. MUSICA",   "CHIPTUNE",  COLOR_GREEN);
    draw_desktop_icon(ic_x1, 320, 5, "6. TAREFAS",  "MULTITASK", COLOR_WHITE);
    draw_desktop_icon(ic_x1, 380, 6, "7. PAINT",    "BMP ESTUDIO",COLOR_WHITE);
    draw_desktop_icon(ic_x1, 440, 7, "8. EXPLORAR", "SINTAXE #|",COLOR_YELLOW);
    draw_desktop_icon(ic_x1, 500, 9, "9. VIDEO",    "BGIF PLAYER",COLOR_PURPLE);
    
    int ic_x2 = 145;
    draw_desktop_icon(ic_x2, 20,  10, "10. CALC", "CALCULADORA", COLOR_ORANGE);
    draw_desktop_icon(ic_x2, 80,  11, "11. LOGS", "CHANGELOG",   COLOR_GREEN);
    draw_desktop_icon(ic_x2, 140, 12, "12. NOTAS", "CAPIVARAPAD",COLOR_YELLOW);

    gfx_draw_rect_alpha(0, screen_h - 40, screen_w, 40, COLOR_NAVY, 215);
    gfx_draw_rect(10, screen_h - 35, 80, 30, start_menu_open ? COLOR_GREEN : COLOR_BLUE);
    gfx_draw_string("START", 30, screen_h - 24, COLOR_NAVY);

    for (int z = 1; z <= top_z_index; z++) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z_index == z) draw_single_window(&windows[i]);
        }
    }

    gfx_draw_cursor(mouse_x, mouse_y);
    gfx_swap_buffers();
}
