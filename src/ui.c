#include "../include/ui.h"
#include "../include/gfx.h"
#include "../include/util.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/vfs.h"
#include "../include/bmp.h"
#include "../include/task.h"
#include "../include/sound.h"

#define MAX_WINDOWS 7

typedef struct { int id; int app; int x, y, w, h; int is_open; int z; } window_t;
static window_t windows[MAX_WINDOWS];
static int top_z = 0, drag_id = -1, off_x = 0, off_y = 0;
static int wallpaper = -1, start_open = 0, prev_mouse = 0;

static char input_buf[32]; static int input_idx = 0;
static char shell_out[128] = "VFS PRONTO! DIGITE LS";

// CANVAS INTERNO DO PAINT (160x120 para economizar RAM, sera escalado na tela para 480x360)
static uint32_t paint_canvas[160 * 120];
static uint32_t paint_color = 0xFFFFFF;

void bring_to_front(int id) {
    windows[id].z = ++top_z; windows[id].is_open = 1;
}

void ui_init(void) {
    kmemset(paint_canvas, 0, sizeof(paint_canvas)); // Fundo preto
    kmemset(input_buf, 0, 32);
    windows[0] = (window_t){0, 0, 180, 40, 580, 480, 0, 1};
    windows[1] = (window_t){1, 1, 140, 60, 580, 480, 0, 2};
    windows[2] = (window_t){2, 2, 100, 80, 580, 480, 0, 3};
    windows[3] = (window_t){3, 3, 120, 30, 580, 500, 0, 4};
    windows[4] = (window_t){4, 5, 160, 40, 600, 460, 0, 5};
    windows[5] = (window_t){5, 6, 90,  50, 540, 480, 1, 6}; // PAINT (Abre primeiro!)
    top_z = 6;
}

void dummy_task(void) { while(1) { asm volatile("nop"); } }

void ui_handle_mouse(void) {
    int click = mouse_left_clicked;
    int jp = (click && !prev_mouse); prev_mouse = click;

    if (jp && mouse_x >= 20 && mouse_x <= 150) {
        if (mouse_y >= 30 && mouse_y <= 100) bring_to_front(0);
        else if (mouse_y >= 120 && mouse_y <= 190) bring_to_front(1);
        else if (mouse_y >= 210 && mouse_y <= 280) bring_to_front(2);
        else if (mouse_y >= 300 && mouse_y <= 370) bring_to_front(3);
        else if (mouse_y >= 390 && mouse_y <= 460) bring_to_front(4);
        else if (mouse_y >= 480 && mouse_y <= 550) bring_to_front(5); // Paint
    }

    if (jp && mouse_x >= 10 && mouse_x <= 90 && mouse_y >= 565 && mouse_y <= 595) { start_open = !start_open; return; }

    if (start_open && jp) start_open = 0;

    int top_id = -1, max_z = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].is_open && mouse_x >= windows[i].x && mouse_x <= windows[i].x + windows[i].w && mouse_y >= windows[i].y && mouse_y <= windows[i].y + windows[i].h) {
            if (windows[i].z > max_z) { max_z = windows[i].z; top_id = i; }
        }
    }

    if (jp && top_id != -1) {
        bring_to_front(top_id);
        window_t* w = &windows[top_id];

        if (mouse_y <= w->y + 30) {
            if (mouse_x >= w->x + w->w - 25) { w->is_open = 0; return; }
            drag_id = top_id; off_x = mouse_x - w->x; off_y = mouse_y - w->y;
        }

        // Criar Task no Gerenciador
        if (w->app == 5 && mouse_y >= w->y + 400 && mouse_y <= w->y + 430 && mouse_x >= w->x + 40 && mouse_x <= w->x + 200) {
            task_create(dummy_task, "Nova_Tarefa", 3);
            sound_click();
        }

        // PAINT CONTROLES
        if (w->app == 6) {
            int py = w->y + 430;
            if (mouse_y >= py && mouse_y <= py + 30) {
                if (mouse_x >= w->x+20 && mouse_x <= w->x+60) paint_color = 0xFF0000;
                else if (mouse_x >= w->x+70 && mouse_x <= w->x+110) paint_color = 0x00FF00;
                else if (mouse_x >= w->x+120 && mouse_x <= w->x+160) paint_color = 0x0000FF;
                else if (mouse_x >= w->x+170 && mouse_x <= w->x+210) paint_color = 0xFFFFFF;
                else if (mouse_x >= w->x+220 && mouse_x <= w->x+260) paint_color = 0x000000;
                else if (mouse_x >= w->x+350 && mouse_x <= w->x+510) {
                    // BOSS 3: GERAÇÃO E EXPORTAÇÃO DE ARQUIVO .BMP NO DISCO VFS!
                    sound_click();
                    bmp_export("arte.bmp", paint_canvas, 160, 120);
                }
            }
        }
    }

    // DRAWING NO PAINT (Arrastando o mouse)
    if (click && top_id != -1 && windows[top_id].app == 6) {
        window_t* w = &windows[top_id];
        int c_x = w->x + 30, c_y = w->y + 60;
        if (mouse_x >= c_x && mouse_x < c_x + 480 && mouse_y >= c_y && mouse_y < c_y + 360) {
            // Mapeia o mouse (escala x3 na tela) para o canvas real (160x120)
            int p_x = (mouse_x - c_x) / 3;
            int p_y = (mouse_y - c_y) / 3;
            // Pincel grosso
            for(int dy=-1; dy<=1; dy++) {
                for(int dx=-1; dx<=1; dx++) {
                    if (p_x+dx >= 0 && p_x+dx < 160 && p_y+dy >= 0 && p_y+dy < 120)
                        paint_canvas[(p_y+dy) * 160 + (p_x+dx)] = paint_color;
                }
            }
        }
    }

    if (click && drag_id != -1) {
        window_t* w = &windows[drag_id];
        w->x = mouse_x - off_x; w->y = mouse_y - off_y;
    } else drag_id = -1;
}

void ui_handle_keyboard(void) {
    if (last_key_pressed) {
        char c = last_key_pressed; last_key_pressed = 0;
        if (c == '\b') { if (input_idx > 0) input_buf[--input_idx] = '\0'; }
        else if (c == '\n') {
            size_t size;
            if (kstrncmp(input_buf, "write ", 6) == 0) {
                char fn[32]; int idx=0, i=6;
                while(input_buf[i]!=' ' && input_buf[i] && idx<31) fn[idx++] = input_buf[i++];
                fn[idx] = '\0'; if (input_buf[i]==' ') i++;
                vfs_write_file(fn, (const uint8_t*)(input_buf+i), kstrlen(input_buf+i));
                kstrcpy(shell_out, "GRAVADO NO HD!");
            } else if (kstrncmp(input_buf, "cat ", 4) == 0) {
                const uint8_t* f = vfs_read(input_buf+4, &size);
                if (f) { kmemcpy(shell_out, f, size < 120 ? size : 120); shell_out[size] = '\0'; }
            } else if (kstrcmp(input_buf, "ls") == 0) {
                vfs_list(shell_out, 128);
            }
            input_idx = 0; input_buf[0] = '\0';
        } else if (input_idx < 30) {
            input_buf[input_idx++] = c; input_buf[input_idx] = '\0';
        }
    }
}

void draw_single_window(window_t* w) {
    gfx_draw_rect(w->x + 8, w->y + 8, w->w, w->h, 0x11111B);
    gfx_draw_rect(w->x, w->y, w->w, w->h, COLOR_GRAY);
    gfx_draw_rect(w->x, w->y, w->w, 30, COLOR_LIGHT_GRAY);
    gfx_draw_rect(w->x + w->w - 25, w->y + 7, 16, 16, COLOR_RED);

    if (w->app == 0) {
        gfx_draw_string("TERMINAL SHELL VFS (HD):", w->x + 15, w->y + 11, COLOR_WHITE);
        gfx_draw_rect(w->x + 30, w->y + 105, w->w - 60, 35, COLOR_NAVY);
        gfx_draw_string("myos> ", w->x + 40, w->y + 118, COLOR_GREEN);
        gfx_draw_string(input_buf, w->x + 90, w->y + 118, COLOR_WHITE);
        gfx_draw_string(shell_out, w->x + 50, w->y + 220, COLOR_WHITE);
    } else if (w->app == 3) {
        gfx_draw_string("LEITOR DE .BMP E GALERIA", w->x + 15, w->y + 11, COLOR_WHITE);
        size_t s; const uint8_t* bmp = vfs_read("arte.bmp", &s);
        if (!bmp) bmp = vfs_read("foto.bmp", &s);
        if (bmp) bmp_draw(bmp, w->x+30, w->y+65, 520, 310);
    } else if (w->app == 5) {
        gfx_draw_string("GERENCIADOR DE TAREFAS", w->x + 15, w->y + 11, COLOR_WHITE);
        gfx_draw_rect(w->x + 30, w->y + 70, w->w - 60, 310, COLOR_NAVY);
        int num_t = get_num_tasks();
        for (int i = 0; i < num_t; i++) {
            task_t* t = get_task(i);
            gfx_draw_number(t->pid, w->x + 40, w->y + 100 + (i * 30), COLOR_WHITE);
            gfx_draw_string(t->name, w->x + 90, w->y + 100 + (i * 30), COLOR_WHITE);
            gfx_draw_number(t->time_ticks, w->x + 400, w->y + 100 + (i * 30), COLOR_GREEN);
        }
        gfx_draw_rect(w->x + 40, w->y + 400, 160, 30, COLOR_GREEN);
        gfx_draw_string("+ CRIAR PROCESSO", w->x + 50, w->y + 410, COLOR_NAVY);
    } else if (w->app == 6) {
        // APP 6: PAINT E EXPORTAÇÃO BMP
        gfx_draw_string("APP PAINT: DESENHE E EXPORTE (BMP)", w->x + 15, w->y + 11, COLOR_WHITE);
        
        // Desenha o Canvas esticado em escala 3x
        int cx = w->x + 30, cy = w->y + 60;
        for (int py = 0; py < 360; py++) {
            for (int px = 0; px < 480; px++) {
                uint32_t color = paint_canvas[(py / 3) * 160 + (px / 3)];
                gfx_put_pixel(cx + px, cy + py, color);
            }
        }
        
        // Paleta de Cores
        int py = w->y + 430;
        gfx_draw_rect(w->x+20, py, 40, 30, 0xFF0000);
        gfx_draw_rect(w->x+70, py, 40, 30, 0x00FF00);
        gfx_draw_rect(w->x+120, py, 40, 30, 0x0000FF);
        gfx_draw_rect(w->x+170, py, 40, 30, 0xFFFFFF);
        gfx_draw_rect(w->x+220, py, 40, 30, 0x000000);

        // Botão Exportar BMP
        gfx_draw_rect(w->x+350, py, 160, 30, COLOR_GREEN);
        gfx_draw_string("EXPORTAR .BMP HD", w->x+360, py+10, COLOR_NAVY);
    }
}

void ui_render(void) {
    gfx_clear(COLOR_DARK_SLATE);
    gfx_draw_rect(20, 30, 120, 60, COLOR_NAVY); gfx_draw_string("1. SHELL", 35, 41, COLOR_WHITE);
    gfx_draw_rect(20, 120, 120, 60, COLOR_NAVY); gfx_draw_string("2. MEMORIA", 35, 131, COLOR_WHITE);
    gfx_draw_rect(20, 210, 120, 60, COLOR_NAVY); gfx_draw_string("3. HARDWARE", 35, 221, COLOR_WHITE);
    gfx_draw_rect(20, 300, 120, 60, COLOR_NAVY); gfx_draw_string("4. GALERIA", 35, 311, COLOR_WHITE);
    gfx_draw_rect(20, 390, 120, 60, COLOR_NAVY); gfx_draw_string("5. TAREFAS", 35, 401, COLOR_WHITE);
    gfx_draw_rect(20, 480, 120, 60, COLOR_RED); gfx_draw_string("6. PAINT", 35, 491, COLOR_WHITE);

    gfx_draw_rect(0, 560, 800, 40, COLOR_NAVY);
    gfx_draw_rect(10, 565, 80, 30, COLOR_BLUE); gfx_draw_string("START", 30, 576, COLOR_NAVY);

    for (int z = 1; z <= top_z; z++) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].is_open && windows[i].z == z) draw_single_window(&windows[i]);
        }
    }
    gfx_draw_cursor(mouse_x, mouse_y);
    gfx_swap_buffers();
}
