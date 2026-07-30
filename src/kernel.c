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
#include "../include/net.h"

void task_music_loop(void) {
    while (1) {
        music_update();
        asm volatile ("hlt");
    }
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL BARE-METAL (FAT32 & RTL8139 NET)\n");

    memory_init(mbi);
    gfx_init(mbi);
    idt_init();
    vfs_init(); // Inicializa FAT32 Real no HD
    ui_init();  // Inicializa Placa de Rede e UI

    task_init();
    task_create(task_music_loop, "Chiptune_Synthesizer", 2);

    sound_startup();
    serial_write("[LOG SERIAL] Placa de Rede e FAT32 Ativos no Kernel 64-bit!\n");

    while (1) {
        ui_handle_mouse();
        ui_handle_keyboard();

        // POLL DE REDE: Processa e responde PINGs automaticamente!
        net_poll();

        ui_render();
        asm volatile ("hlt");
    }
}
