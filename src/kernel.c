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
#include "../include/syscall.h"
#include "../include/klibc.h"

void task_music_loop(void) {
    while (1) {
        music_update();
        task_yield();
    }
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] KERNEL CAPIVARAOS 64-BIT ADVANCED (SERIAL BIDIRECIONAL ATIVO)\n");

    memory_init(mbi);
    vmm_init();
    
    gfx_init(mbi);
    vfs_init();
    ui_init();

    syscall_init();
    task_init();
    idt_init();

    task_create(task_music_loop, "Chiptune_Synthesizer", 3);

    // Habilita interrupções de hardware com segurança
    asm volatile ("sti");

    sound_startup();

    while (1) {
        serial_poll();
        ui_handle_mouse();
        ui_handle_keyboard();
        net_poll();
        ui_render();
        task_yield();
    }
}
