#include "../include/idt.h"
#include "../include/gfx.h"
#include "../include/serial.h"
#include "../include/task.h"

static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }

struct idt_entry {
    uint16_t base_low; uint16_t sel; uint8_t ist; uint8_t flags;
    uint16_t base_mid; uint32_t base_high; uint32_t reserved;
} __attribute__((packed));

struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void load_idt(struct idt_ptr*);
extern void isr0_stub(void);
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);

int mouse_x = 400, mouse_y = 300, mouse_left_clicked = 0;
char last_key_pressed = 0;
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

static int shift_pressed = 0;
static int caps_lock = 0;
static char active_dead_key = 0; // GUARDA O ACENTO ATIVO (', ~, ^, `)

static const char scancode_ascii_normal[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

static const char scancode_ascii_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
};

// MAQUINA DE FUSAO DE ACENTOS (DEAD KEY + LETRA)
static char apply_accent(char dead, char base) {
    if (dead == '\'') {
        if (base == 'a') return (char)225; // á
        if (base == 'e') return (char)233; // é
        if (base == 'i') return (char)237; // í
        if (base == 'o') return (char)243; // ó
        if (base == 'u') return (char)250; // ú
        if (base == 'c') return (char)231; // ç
        if (base == 'A') return (char)193; // Á
        if (base == 'E') return (char)201; // É
        if (base == 'I') return (char)205; // Í
        if (base == 'O') return (char)211; // Ó
        if (base == 'U') return (char)218; // Ú
        if (base == 'C') return (char)199; // Ç
    } else if (dead == '~') {
        if (base == 'a') return (char)227; // ã
        if (base == 'o') return (char)245; // õ
        if (base == 'A') return (char)195; // Ã
        if (base == 'O') return (char)213; // Õ
    } else if (dead == '^') {
        if (base == 'a') return (char)226; // â
        if (base == 'e') return (char)234; // ê
        if (base == 'o') return (char)244; // ô
        if (base == 'A') return (char)194; // Â
        if (base == 'E') return (char)202; // Ê
        if (base == 'O') return (char)212; // Ô
    } else if (dead == '`') {
        if (base == 'a') return (char)224; // à
        if (base == 'A') return (char)192; // À
    }
    return base;
}

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_mid = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel; idt[num].ist = 0; idt[num].flags = flags; idt[num].reserved = 0;
}

void exception0_handler(void) {
    serial_write("[KERNEL PANIC] DIVISAO POR ZERO EM 64-BITS!\n");
    gfx_clear(0x880000);
    gfx_draw_rect(100, 150, 600, 40, 0xFFFFFF);
    gfx_draw_string("KERNEL PANIC: EXCECAO 00 (DIVISAO POR ZERO)", 120, 162, 0x880000);
    gfx_swap_buffers();
    while (1) { asm volatile ("hlt"); }
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02); outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xF8); outb(0xA1, 0xEF);
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
    mouse_wait(1); outb(0x64, 0xD4);
    mouse_wait(1); outb(0x60, data);
}

uint8_t mouse_read(void) {
    mouse_wait(0); return inb(0x60);
}

void mouse_init(void) {
    uint8_t status;
    mouse_wait(1); outb(0x64, 0xA8);
    mouse_wait(1); outb(0x64, 0x20);
    mouse_wait(0); status = (inb(0x60) | 2);
    mouse_wait(1); outb(0x64, 0x60);
    mouse_wait(1); outb(0x60, status);
    mouse_write(0xF4); mouse_read();
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
    while (inb(0x64) & 0x01) {
        uint8_t scancode = inb(0x60);
        
        if (scancode & 0x80) { // Soltura de tecla
            uint8_t released = scancode & 0x7F;
            if (released == 0x2A || released == 0x36) {
                shift_pressed = 0;
            }
        } else { // Pressionada
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
            } else if (scancode == 0x3A) {
                caps_lock = !caps_lock;
            } else if (scancode < 128) {
                char raw_c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii_normal[scancode];
                
                if (!shift_pressed && caps_lock && raw_c >= 'a' && raw_c <= 'z') {
                    raw_c -= 32;
                }

                // PROCESSAMENTO DE TECLAS MORTAS (DEAD KEYS)
                if (raw_c == '\'' || raw_c == '~' || raw_c == '^' || raw_c == '`') {
                    if (active_dead_key == 0) {
                        active_dead_key = raw_c; // Guarda o acento e aguarda a letra seguinte
                        continue;
                    } else if (active_dead_key == raw_c) { // Digitou o acento duas vezes (ex: '')
                        last_key_pressed = raw_c;
                        active_dead_key = 0;
                        continue;
                    }
                }

                if (raw_c != 0) {
                    if (active_dead_key != 0) {
                        last_key_pressed = apply_accent(active_dead_key, raw_c);
                        active_dead_key = 0;
                    } else {
                        last_key_pressed = raw_c;
                    }
                }
            }
        }
    }
    outb(0x20, 0x20);
}

void mouse_handler_main(void) {
    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        if (!(status & 0x20)) {
            keyboard_handler_main();
            continue;
        }

        uint8_t data = inb(0x60);
        switch (mouse_cycle) {
            case 0:
                if (data & 0x08) {
                    mouse_byte[0] = data;
                    mouse_cycle++;
                }
                break;
            case 1:
                mouse_byte[1] = data;
                mouse_cycle++;
                break;
            case 2:
                mouse_byte[2] = data;
                mouse_cycle = 0;

                mouse_left_clicked = (mouse_byte[0] & 0x01);

                int dx = (int8_t)mouse_byte[1];
                int dy = (int8_t)mouse_byte[2];

                mouse_x += dx;
                mouse_y -= dy;

                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= 1024) mouse_x = 1023;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= 768) mouse_y = 767;
                break;
        }
    }
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
