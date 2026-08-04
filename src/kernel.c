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

void task_music_loop(void) {
    while (1) {
        music_update();
        task_yield(); // Otimização para E/S e Multitarefas
    }
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] KERNEL CAPIVARAOS 64-BIT ADVANCED (MLFQ + VMM) \n");

    memory_init(mbi);
    vmm_init(); // Ativa Proteção de Paginação PML4
    
    gfx_init(mbi);
    idt_init();
    vfs_init();
    ui_init();

    syscall_init();

    task_init();
    task_create(task_music_loop, "Chiptune_Synthesizer", 3); // Prioridade 3

    sound_startup();
    serial_write("[LOG SERIAL] Excecoes da IDT Protegidas contra Triple-Fault!\n");

    while (1) {
        ui_handle_mouse();
        ui_handle_keyboard();
        net_poll();
        ui_render();
        task_yield(); // Cedendo ciclos ociosos para o Escalonador Assíncrono (-O3 Friendly)
    }
}
