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
        asm volatile ("hlt");
    }
}

void kernel_main(multiboot_info_t* mbi) {
    serial_init();
    serial_write("[LOG SERIAL] INICIALIZANDO KERNEL CAPIVARAOS 64-BIT v1.0 (BOSS 6 & APP 10)\n");

    memory_init(mbi);
    gfx_init(mbi);
    idt_init();
    vfs_init();
    ui_init();

    // INICIALIZA AS INSTRUÇÕES MSR DE SYSCALL/SYSRET DA CPU DE 64-BITS
    syscall_init();

    task_init();
    task_create(task_music_loop, "Chiptune_Synthesizer", 2);

    sound_startup();
    serial_write("[LOG SERIAL] Syscalls e Explorador #| Ativos no Kernel 64-bit!\n");

    while (1) {
        ui_handle_mouse();
        ui_handle_keyboard();
        net_poll();
        ui_render();
        asm volatile ("hlt");
    }
}
