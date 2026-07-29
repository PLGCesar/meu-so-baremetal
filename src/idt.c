#include "../include/idt.h"
#include "../include/gfx.h"
#include "../include/serial.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void load_idt(struct idt_ptr*);
extern void isr0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);

int mouse_x = 400;
int mouse_y = 300;
int mouse_left_clicked = 0;
char last_key_pressed = 0;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

// HANDLER DE ERRO DA CPU: KERNEL PANIC
void exception0_handler(void) {
    serial_write("[KERNEL PANIC] EXCECAO 00 CAPTURADA: DIVISAO POR ZERO!\n");
    gfx_clear(0x880000); // Tela vermelha de erro
    gfx_draw_rect(100, 150, 600, 300, 0x11111B);
    gfx_draw_rect(100, 150, 600, 40, COLOR_RED);
    gfx_draw_string("KERNEL PANIC: EXCECAO 00 (DIVISAO POR ZERO)", 120, 162, COLOR_WHITE);
    gfx_draw_string("O Kernel capturou uma falha critica de hardware.", 120, 220, COLOR_WHITE);
    gfx_draw_string("O sistema foi interrompido com seguranca para evitar danos.", 120, 250, COLOR_WHITE);
    gfx_draw_string("Aperte RESET no QEMU para reiniciar o sistema.", 120, 320, COLOR_GREEN);
    gfx_swap_buffers();
    while (1) { asm volatile ("hlt"); }
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);

    outb(0x21, 0xF9);
    outb(0xA1, 0xEF);
}

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init(void) {
    uint8_t status;
    mouse_wait(1);
    outb(0x64, 0xA8);
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF4);
    mouse_read();
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);

    pic_remap();

    // EXCEÇÕES E INTERRUPÇÕES COM SELETOR 0x10
    idt_set_gate(0,  (uint32_t)isr0_stub,  0x10, 0x8E); // Exception 0: Divisao por Zero
    idt_set_gate(33, (uint32_t)irq1_stub,  0x10, 0x8E); // IRQ1: Teclado
    idt_set_gate(44, (uint32_t)irq12_stub, 0x10, 0x8E); // IRQ12: Mouse

    load_idt(&idtp);
    mouse_init();
}

const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

void keyboard_handler_main(void) {
    uint8_t scancode = inb(0x60);
    outb(0x20, 0x20);

    if (!(scancode & 0x80)) {
        char c = scancode_ascii[scancode];
        if (c != 0) last_key_pressed = c;
    }
}

void mouse_handler_main(void) {
    uint8_t status = inb(0x64);
    if (!(status & 1)) return;

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            if (data & 0x08) { mouse_byte[0] = data; mouse_cycle++; }
            break;
        case 1:
            mouse_byte[1] = data; mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = data; mouse_cycle = 0;
            mouse_left_clicked = (mouse_byte[0] & 0x01);

            int delta_x = mouse_byte[1];
            int delta_y = mouse_byte[2];

            mouse_x += delta_x;
            mouse_y -= delta_y;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= 800) mouse_x = 799;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= 600) mouse_y = 599;
            break;
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
