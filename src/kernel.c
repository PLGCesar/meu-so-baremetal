#include "../include/multiboot.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/vfs.h"
#include "../include/sound.h"
#include "../include/music.h"
#include "../include/ui.h"
#include "../include/task.h"

// PROCESSO SECUNDÁRIO RODANDO EM PARALELO (SINTETIZADOR DE MÚSICA)
void task_music_loop(void) {
    while (1) {
        music_update();
        asm volatile ("hlt");
    }
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL BARE-METAL v1.0 (MULTITASKING)\n");

    memory_init(mbi);
    gfx_init(mbi);
    idt_init();
    vfs_init();
    ui_init();

    // 1. Inicializa o Escalonador registrando o kernel_main como Task 0 (PID 1)
    task_init();

    // 2. Cria a Task 1 (Sintetizador de Música em segundo plano)
    task_create(task_music_loop, "Chiptune_Synthesizer", 2);

    sound_startup();
    serial_write("[LOG SERIAL] Escalonador Preemptivo ativado! Renderizando GUI...\n");

    // LOOP PRINCIPAL DA GUI (Task 0 - PID 1)
    while (1) {
        ui_handle_mouse();
        ui_handle_keyboard();
        ui_render();
        asm volatile ("hlt");
    }
}
