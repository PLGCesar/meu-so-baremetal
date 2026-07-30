#include "../include/idt.h"
#include "../include/gfx.h"
#include "../include/serial.h"
#include "../include/task.h"

static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }

// ESTRUTURA IDT DE 64-BITS (MUITO MAIOR E COMPLEXA)
struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void load_idt(struct idt_ptr*);
extern void isr0_stub(void);
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);

int mouse_x = 400, mouse_y = 300, mouse_left_clicked = 0;
char last_key_pressed = 0;
static uint8_t mouse_cycle = 0; static int8_t mouse_byte[3];

const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

// MAPEIA A FUNÇÃO DE C 64-BITS NA TABELA
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_mid = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

void exception0_handler(void) {
    serial_write("[KERNEL PANIC] DIVISAO POR ZERO EM 64-BITS!\n");
    gfx_clear(0x880000);
    gfx_draw_rect(100, 150, 600, 40, 0xFFFFFF);
    gfx_draw_string("KERNEL PANIC: EXCECAO 00 (DIVISAO POR ZERO) EM 64-BITS", 120, 162, 0x880000);
    gfx_swap_buffers();
    while (1) { asm volatile ("hlt"); }
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02); outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xF8); outb(0xA1, 0xEF);
}

void mouse_init(void) {
    outb(0x64, 0xA8); outb(0x64, 0x20); outb(0x64, 0x60);
    outb(0x60, (inb(0x60) | 2)); outb(0x60, 0xF4);
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint64_t)&idt;
    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);
    pic_remap();
    
    idt_set_gate(0,  (uint64_t)isr0_stub,  0x08, 0x8E);
    idt_set_gate(32, (uint64_t)irq0_stub,  0x08, 0x8E);
    idt_set_gate(33, (uint64_t)irq1_stub,  0x08, 0x8E);
    idt_set_gate(44, (uint64_t)irq12_stub, 0x08, 0x8E);

    load_idt(&idtp);
    timer_init();
    mouse_init();
}

void keyboard_handler_main(void) {
    uint8_t s = inb(0x60); outb(0x20, 0x20);
    if (!(s & 0x80) && scancode_ascii[s] != 0) last_key_pressed = scancode_ascii[s];
}

void mouse_handler_main(void) {
    if (!(inb(0x64) & 1)) return;
    uint8_t data = inb(0x60);
    switch (mouse_cycle) {
        case 0: if (data & 0x08) { mouse_byte[0] = data; mouse_cycle++; } break;
        case 1: mouse_byte[1] = data; mouse_cycle++; break;
        case 2: mouse_byte[2] = data; mouse_cycle = 0;
            mouse_left_clicked = (mouse_byte[0] & 0x01);
            mouse_x += mouse_byte[1]; mouse_y -= mouse_byte[2];
            if (mouse_x < 0) mouse_x = 0; if (mouse_x >= 800) mouse_x = 799;
            if (mouse_y < 0) mouse_y = 0; if (mouse_y >= 600) mouse_y = 599;
            break;
    }
    outb(0xA0, 0x20); outb(0x20, 0x20);
}
