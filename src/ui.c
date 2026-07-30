#include "../include/ui.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/rtc.h"
#include "../include/vfs.h"
#include "../include/sound.h"
#include "../include/music.h"

static int current_wallpaper = -1;
static int win_x = 80, win_y = 30, win_w = 640, win_h = 520;
static int win_open = 1;
static int is_dragging = 0;
static int drag_off_x = 0, drag_off_y = 0;

static int start_menu_open = 0;
static int active_app = 4; // 0 = Shell, 1 = Memoria, 2 = IDT, 3 = Galeria, 4 = Player de Musica

static int gallery_photo = 0;
static char gallery_status_msg[64] = "CLIQUE NOS BOTOES PARA USAR COMO WALLPAPER OU SALVAR!";

static char input_buffer[32];
static int input_index = 0;
static char shell_output[128] = "SISTEMA VFS INICIALIZADO! DIGITE 'LS' OU 'HELP'.";

static int prev_mouse_left = 0;

static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int kstrncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void format_time_string(char* buf, uint8_t h, uint8_t m, uint8_t s) {
    buf[0] = '0' + (h / 10); buf[1] = '0' + (h % 10); buf[2] = ':';
    buf[3] = '0' + (m / 10); buf[4] = '0' + (m % 10); buf[5] = ':';
    buf[6] = '0' + (s / 10); buf[7] = '0' + (s % 10); buf[8] = ' ';
    buf[9] = 'B'; buf[10] = 'R'; buf[11] = 'T'; buf[12] = '\0';
}

void ui_init(void) {
    music_init();
    input_buffer[0] = '\0';
}

static void process_shell_command(void) {
    for (int i = 0; i < 128; i++) shell_output[i] = '\0';

    if (kstrcmp(input_buffer, "help") == 0) {
        const char* msg = "COMANDOS: LS, CAT <ARQ>, WRITE <ARQ> <TEXTO>, PANIC";
        for (int i = 0; msg[i] != '\0'; i++) shell_output[i] = msg[i];
    } else if (kstrcmp(input_buffer, "ls") == 0) {
        vfs_list(shell_output, 128);
    } else if (kstrncmp(input_buffer, "cat ", 4) == 0) {
        const char* content = vfs_read(input_buffer + 4);
        if (content) {
            for (int i = 0; content[i] != '\0' && i < 120; i++) shell_output[i] = content[i];
        }
    } else if (kstrncmp(input_buffer, "write ", 6) == 0) {
        char filename[32]; int f_idx = 0, i = 6;
        while (input_buffer[i] != ' ' && input_buffer[i] != '\0' && f_idx < 31) {
            filename[f_idx++] = input_buffer[i++];
        }
        filename[f_idx] = '\0';
        if (input_buffer[i] == ' ') i++;
        if (vfs_write_file(filename, input_buffer + i)) {
            const char* msg = "ARQUIVO GRAVADO COM SUCESSO!";
            for (int j = 0; msg[j] != '\0'; j++) shell_output[j] = msg[j];
        }
    } else if (kstrcmp(input_buffer, "clear") == 0) {
        shell_output[0] = '\0';
    } else if (kstrcmp(input_buffer, "panic") == 0) {
        int a = 10, b = 0, c = a / b; (void)c;
    }
    input_index = 0;
    input_buffer[0] = '\0';
}

void ui_handle_keyboard(void) {
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
}

void ui_handle_mouse(void) {
    int click = mouse_left_clicked;
    int just_pressed = (click && !prev_mouse_left);
    prev_mouse_left = click;

    if (just_pressed) sound_click();

    // 1. Ícones do Desktop
    if (just_pressed && mouse_x >= 20 && mouse_x <= 150) {
        if (mouse_y >= 30 && mouse_y <= 100) { active_app = 0; win_open = 1; }
        else if (mouse_y >= 120 && mouse_y <= 190) { active_app = 1; win_open = 1; }
        else if (mouse_y >= 210 && mouse_y <= 280) { active_app = 2; win_open = 1; }
        else if (mouse_y >= 300 && mouse_y <= 370) { active_app = 3; win_open = 1; }
        else if (mouse_y >= 390 && mouse_y <= 460) { active_app = 4; win_open = 1; }
    }

    // 2. Botão START
    if (just_pressed && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= 565 && mouse_y <= 595) {
        start_menu_open = !start_menu_open;
        return;
    }

    // 3. Menu Start
    if (start_menu_open && just_pressed && mouse_x >= 10 && mouse_x <= 200 && mouse_y >= 350 && mouse_y <= 560) {
        if (mouse_y >= 360 && mouse_y < 395) { active_app = 0; win_open = 1; }
        else if (mouse_y >= 395 && mouse_y < 430) { active_app = 1; win_open = 1; }
        else if (mouse_y >= 430 && mouse_y < 465) { active_app = 2; win_open = 1; }
        else if (mouse_y >= 465 && mouse_y < 500) { active_app = 3; win_open = 1; }
        else if (mouse_y >= 500 && mouse_y <= 550) { active_app = 4; win_open = 1; }
        start_menu_open = 0;
        return;
    }

    if (start_menu_open && just_pressed) start_menu_open = 0;
    if (!win_open) return;

    // 4. Fechar Janela [X]
    if (just_pressed && mouse_x >= (win_x + win_w - 25) && mouse_x <= (win_x + win_w - 9) &&
        mouse_y >= (win_y + 7) && mouse_y <= (win_y + 23)) {
        win_open = 0; is_dragging = 0; return;
    }

    // 5. Player de Música
    if (win_open && active_app == 4 && just_pressed) {
        if (mouse_y >= win_y + 160 && mouse_y <= win_y + 200) {
            if (mouse_x >= win_x + 50 && mouse_x <= win_x + 200) {
                music_toggle_play();
            } else if (mouse_x >= win_x + 220 && mouse_x <= win_x + 400) {
                music_next_track();
            }
        }
    }

    // 6. Galeria
    if (win_open && active_app == 3 && just_pressed) {
        int btn_y1 = win_y + 390;
        int btn_y2 = win_y + 430;

        if (mouse_y >= btn_y1 && mouse_y <= btn_y1 + 30) {
            if (mouse_x >= win_x + 20 && mouse_x <= win_x + 130) gallery_photo = 0;
            else if (mouse_x >= win_x + 140 && mouse_x <= win_x + 230) gallery_photo = 1;
            else if (mouse_x >= win_x + 240 && mouse_x <= win_x + 350) gallery_photo = 2;
        }

        if (mouse_y >= btn_y2 && mouse_y <= btn_y2 + 30) {
            if (mouse_x >= win_x + 20 && mouse_x <= win_x + 210) current_wallpaper = gallery_photo;
            else if (mouse_x >= win_x + 225 && mouse_x <= win_x + 435) vfs_write_file("paisagem.art", "ARTE RECENTE DA GALERIA");
            else if (mouse_x >= win_x + 450 && mouse_x <= win_x + 610) current_wallpaper = -1;
        }
    }

    // 7. Arrasto da Janela
    if (just_pressed && mouse_x >= win_x && mouse_x <= (win_x + win_w - 50) &&
        mouse_y >= win_y && mouse_y <= (win_y + 30)) {
        is_dragging = 1; drag_off_x = mouse_x - win_x; drag_off_y = mouse_y - win_y;
    }

    if (click && is_dragging) {
        win_x = mouse_x - drag_off_x; win_y = mouse_y - drag_off_y;
        if (win_x < 0) win_x = 0;
        if (win_x + win_w > 800) win_x = 800 - win_w;
        if (win_y < 0) win_y = 0;
        if (win_y + win_h > 555) win_y = 555 - win_h;
    }

    if (!click) is_dragging = 0;
}

void ui_render(void) {
    if (current_wallpaper == 0) gfx_draw_landscape_sunset(0, 0, 800, 600);
    else if (current_wallpaper == 1) gfx_draw_landscape_cosmos(0, 0, 800, 600);
    else if (current_wallpaper == 2) gfx_draw_landscape_synthwave(0, 0, 800, 600);
    else gfx_clear(COLOR_DARK_SLATE);

    // ÍCONES DO DESKTOP
    gfx_draw_rect(20, 30, 120, 60, COLOR_NAVY);
    gfx_draw_rect(25, 35, 110, 20, COLOR_BLUE);
    gfx_draw_string("1. SHELL", 35, 41, COLOR_WHITE);
    gfx_draw_string("DISCO VFS", 35, 68, COLOR_GREEN);

    gfx_draw_rect(20, 120, 120, 60, COLOR_NAVY);
    gfx_draw_rect(25, 125, 110, 20, COLOR_BLUE);
    gfx_draw_string("2. MEMORIA", 35, 131, COLOR_WHITE);
    gfx_draw_string("RAM HEAP", 35, 158, COLOR_GREEN);

    gfx_draw_rect(20, 210, 120, 60, COLOR_NAVY);
    gfx_draw_rect(25, 215, 110, 20, COLOR_BLUE);
    gfx_draw_string("3. HARDWARE", 30, 221, COLOR_WHITE);
    gfx_draw_string("IDT & CPU", 35, 248, COLOR_GREEN);

    gfx_draw_rect(20, 300, 120, 60, COLOR_NAVY);
    gfx_draw_rect(25, 305, 110, 20, COLOR_BLUE);
    gfx_draw_string("4. GALERIA", 35, 311, COLOR_WHITE);
    gfx_draw_string("PINTOR", 35, 338, COLOR_GREEN);

    gfx_draw_rect(20, 390, 120, 60, COLOR_NAVY);
    gfx_draw_rect(25, 395, 110, 20, COLOR_RED);
    gfx_draw_string("5. MUSICA", 35, 401, COLOR_WHITE);
    gfx_draw_string("PLAYER 8BIT", 30, 428, COLOR_GREEN);

    // BARRA DE TAREFAS
    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, start_menu_open ? COLOR_GREEN : COLOR_BLUE);
    gfx_draw_string("START", 30, 576, COLOR_NAVY);

    rtc_time_t clock_brt;
    rtc_get_time_brt(&clock_brt);
    char time_str[16];
    format_time_string(time_str, clock_brt.hour, clock_brt.minute, clock_brt.second);
    gfx_draw_string(time_str, 680, 576, COLOR_WHITE);

    // JANELA PRINCIPAL
    if (win_open) {
        gfx_draw_rect(win_x + 8, win_y + 8, win_w, win_h, 0x11111B);
        gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_GRAY);
        gfx_draw_rect(win_x, win_y, win_w, 30, COLOR_LIGHT_GRAY);

        if (active_app == 0) gfx_draw_string("JANELA: TERMINAL SHELL VFS", win_x + 15, win_y + 11, COLOR_WHITE);
        else if (active_app == 1) gfx_draw_string("JANELA: GERENCIADOR DE MEMORIA RAM", win_x + 15, win_y + 11, COLOR_WHITE);
        else if (active_app == 2) gfx_draw_string("JANELA: DIAGNOSTICO IDT & CPU", win_x + 15, win_y + 11, COLOR_WHITE);
        else if (active_app == 3) gfx_draw_string("JANELA: GALERIA DE PAISAGENS PROCEDURAIS", win_x + 15, win_y + 11, COLOR_WHITE);
        else if (active_app == 4) gfx_draw_string("JANELA: PLAYER DE MUSICA CHIPTUNE 8-BIT", win_x + 15, win_y + 11, COLOR_WHITE);

        gfx_draw_rect(win_x + win_w - 25, win_y + 7, 16, 16, COLOR_RED);
        gfx_draw_rect(win_x + win_w - 45, win_y + 7, 16, 16, COLOR_BLUE);

        if (active_app == 0) {
            gfx_draw_string("TERMINAL SHELL VFS:", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_rect(win_x + 30, win_y + 105, win_w - 60, 35, COLOR_NAVY);
            gfx_draw_string("myos> ", win_x + 40, win_y + 118, COLOR_GREEN);
            gfx_draw_string(input_buffer, win_x + 90, win_y + 118, COLOR_WHITE);
            gfx_draw_rect(win_x + 30, win_y + 160, win_w - 60, 240, COLOR_NAVY);
            gfx_draw_string(shell_output, win_x + 50, win_y + 220, COLOR_WHITE);
        } else if (active_app == 1) {
            gfx_draw_string("GERENCIADOR DE MEMORIA HEAP (RAM):", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 300, COLOR_NAVY);
            gfx_draw_string("RAM ALOCADA ATIVA: ", win_x + 50, win_y + 120, COLOR_WHITE);
            gfx_draw_number((int)memory_get_total_allocated(), win_x + 220, win_y + 120, COLOR_GREEN);
        } else if (active_app == 2) {
            gfx_draw_string("DIAGNOSTICO DE INTERRUPCOES & CPU:", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 300, COLOR_NAVY);
            gfx_draw_string("SINTETIZADOR DE MUSICA 8-BIT: ATIVO", win_x + 50, win_y + 120, COLOR_GREEN);
        } else if (active_app == 3) {
            int canvas_x = win_x + 30, canvas_y = win_y + 65, canvas_w = 520, canvas_h = 310;
            if (gallery_photo == 0) gfx_draw_landscape_sunset(canvas_x, canvas_y, canvas_w, canvas_h);
            else if (gallery_photo == 1) gfx_draw_landscape_cosmos(canvas_x, canvas_y, canvas_w, canvas_h);
            else if (gallery_photo == 2) gfx_draw_landscape_synthwave(canvas_x, canvas_y, canvas_w, canvas_h);

            int btn_y2 = win_y + 430;
            gfx_draw_rect(win_x + 20, btn_y2, 190, 30, COLOR_BLUE);
            gfx_draw_string("USAR COMO WALLPAPER", win_x + 30, btn_y2 + 11, COLOR_WHITE);
            gfx_draw_rect(win_x + 225, btn_y2, 210, 30, COLOR_GREEN);
            gfx_draw_string("SALVAR NO DISCO VFS", win_x + 245, btn_y2 + 11, COLOR_NAVY);
        } else if (active_app == 4) {
            gfx_draw_string("PLAYER DE MUSICA SINTETIZADA CHIPTUNE:", win_x + 30, win_y + 50, COLOR_GREEN);
            gfx_draw_rect(win_x + 30, win_y + 90, win_w - 60, 320, COLOR_NAVY);
            gfx_draw_string("FAIXA ATUAL:", win_x + 50, win_y + 120, COLOR_WHITE);
            gfx_draw_string(music_get_track_name(), win_x + 180, win_y + 120, COLOR_BLUE);

            gfx_draw_rect(win_x + 50, win_y + 160, 150, 40, music_is_playing() ? COLOR_RED : COLOR_GREEN);
            gfx_draw_string(music_is_playing() ? "PAUSAR" : "PLAY", win_x + 95, win_y + 175, COLOR_WHITE);

            gfx_draw_rect(win_x + 220, win_y + 160, 180, 40, COLOR_BLUE);
            gfx_draw_string("MUDAR FAIXA", win_x + 250, win_y + 175, COLOR_WHITE);

            gfx_draw_string("SINTETIZADOR SINTETIZANDO NO PC SPEAKER", win_x + 50, win_y + 240, COLOR_GREEN);
            gfx_draw_string("MUSICA TOCANDO EM SEGUNDO PLANO SEM TRAVAR A GUI!", win_x + 50, win_y + 270, COLOR_WHITE);
        }
    }

    // MENU START POP-UP
    if (start_menu_open) {
        gfx_draw_rect(10, 350, 190, 210, COLOR_NAVY);
        gfx_draw_rect(10, 350, 190, 25, COLOR_BLUE);
        gfx_draw_string("MENU START", 20, 358, COLOR_WHITE);

        gfx_draw_string("> 1. SHELL VFS", 20, 385, active_app == 0 ? COLOR_GREEN : COLOR_WHITE);
        gfx_draw_string("> 2. MEMORIA (RAM)", 20, 420, active_app == 1 ? COLOR_GREEN : COLOR_WHITE);
        gfx_draw_string("> 3. IDT / HARDWARE", 20, 455, active_app == 2 ? COLOR_GREEN : COLOR_WHITE);
        gfx_draw_string("> 4. GALERIA (PINTOR)", 20, 490, active_app == 3 ? COLOR_GREEN : COLOR_WHITE);
        gfx_draw_string("> 5. PLAYER DE MUSICA", 20, 525, active_app == 4 ? COLOR_GREEN : COLOR_WHITE);
    }

    gfx_draw_cursor(mouse_x, mouse_y);
    gfx_swap_buffers();
}
