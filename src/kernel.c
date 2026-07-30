#include "../include/multiboot.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/vfs.h"
#include "../include/sound.h"
#include "../include/music.h"
#include "../include/ui.h"

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL BARE-METAL v1.0\n");

    memory_init(mbi);
    gfx_init(mbi);
    idt_init();
    vfs_init();
    ui_init();

    sound_startup();

    // LOOP PRINCIPAL ULTRALIMPO
    while (1) {
        ui_handle_mouse();
        ui_handle_keyboard();

        // Atualiza a música em segundo plano sem congelar a UI
        music_update();

        ui_render();
        asm volatile ("hlt");
    }
}
