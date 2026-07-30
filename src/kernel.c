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

void task_music_loop(void) {
    while (1) {
        music_update();
        asm volatile ("hlt");
    }
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL BARE-METAL v1.0 (64-BIT COMPLETE)\n");

    memory_init(mbi);
    gfx_init(mbi);
    idt_init();
    vfs_init();
    ui_init();

    task_init();
    task_create(task_music_loop, "Chiptune_Synthesizer", 2);

    sound_startup();
    serial_write("[LOG SERIAL] Sistema 100% Carregado! Executando GUI...\n");

    while (1) {
        ui_handle_mouse();
        ui_handle_keyboard();
        ui_render();
        asm volatile ("hlt");
    }
}
