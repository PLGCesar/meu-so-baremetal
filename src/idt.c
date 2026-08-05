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
extern void default_irq_stub(void);
extern void isr0_stub(void);
extern void isr8_stub(void);
extern void isr13_stub(void);
extern void isr14_stub(void);
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);

int mouse_x = 400, mouse_y = 300, mouse_left_clicked = 0;
char last_key_pressed = 0;
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

static int shift_pressed = 0;
static int caps_lock = 0;
static char active_dead_key = 0;

static const char scancode_ascii_normal[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };
static const char scancode_ascii_shift[128] = { 0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };

static char apply_accent(char dead, char base) {
    if (dead == '\'') {
        if (base == 'a') return (char)225;
        if (base == 'e') return (char)233;
        if (base == 'i') return (char)237;
        if (base == 'o') return (char)243;
        if (base == 'u') return (char)250;
        if (base == 'c') return (char)231;
    } else if (dead == '~') {
        if (base == 'a') return (char)227;
        if (base == 'o') return (char)245;
    } else if (dead == '^') {
        if (base == 'a') return (char)226;
        if (base == 'e') return (char)234;
        if (base == 'o') return (char)244;
    } else if (dead == '`') {
        if (base == 'a') return (char)224;
    }
    return base;
}

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
    serial_write("\n[PANIC CRITICO] EXCECAO 00: DIVISAO POR ZERO\n");
    while (1) asm volatile ("cli; hlt");
}

void exception8_handler(uint64_t error_code) {
    (void)error_code;
    serial_write("\n[PANIC CRITICO] EXCECAO 08: DOUBLE FAULT\n");
    while (1) asm volatile ("cli; hlt");
}

void exception13_handler(uint64_t error_code) {
    (void)error_code;
    serial_write("\n[PANIC CRITICO] EXCECAO 13: GENERAL PROTECTION FAULT (GPF)\n");
    while (1) asm volatile ("cli; hlt");
}

void exception14_handler(uint64_t error_code) {
    (void)error_code;
    serial_write("\n[PANIC CRITICO] EXCECAO 14: PAGE FAULT DETECTADO!\n");
    while (1) asm volatile ("cli; hlt");
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02); outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xF8); outb(0xA1, 0xEF);
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint64_t)&idt;
    for (int i = 0; i < 256; i++) idt_set_gate(i, (uint64_t)default_irq_stub, 0x08, 0x8E);
    pic_remap();
    
    idt_set_gate(0,  (uint64_t)isr0_stub,  0x08, 0x8E);
    idt_set_gate(8,  (uint64_t)isr8_stub,  0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr13_stub, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr14_stub, 0x08, 0x8E);

    idt_set_gate(32, (uint64_t)irq0_stub,  0x08, 0x8E);
    idt_set_gate(33, (uint64_t)irq1_stub,  0x08, 0x8E);
    idt_set_gate(44, (uint64_t)irq12_stub, 0x08, 0x8E);

    load_idt(&idtp);
    
    outb(0x64, 0xA8); outb(0x64, 0x20);
    uint8_t st = inb(0x60) | 2;
    outb(0x64, 0x60); outb(0x60, st);
    outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);
}

void keyboard_handler_main(void) {
    uint8_t status = inb(0x64);
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t scancode = inb(0x60);
        if (scancode & 0x80) {
            uint8_t rel = scancode & 0x7F;
            if (rel == 0x2A || rel == 0x36) shift_pressed = 0;
        } else {
            if (scancode == 0x2A || scancode == 0x36) shift_pressed = 1;
            else if (scancode == 0x3A) caps_lock = !caps_lock;
            else if (scancode < 128) {
                char raw_c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii_normal[scancode];
                if (!shift_pressed && caps_lock && raw_c >= 'a' && raw_c <= 'z') raw_c -= 32;
                if (raw_c == '\'' || raw_c == '~' || raw_c == '^' || raw_c == '`') {
                    if (active_dead_key == 0) { active_dead_key = raw_c; }
                    else if (active_dead_key == raw_c) { last_key_pressed = raw_c; active_dead_key = 0; }
                } else if (raw_c != 0) {
                    if (active_dead_key != 0) { last_key_pressed = apply_accent(active_dead_key, raw_c); active_dead_key = 0; }
                    else last_key_pressed = raw_c;
                }
            }
        }
    }
    outb(0x20, 0x20);
}

void mouse_handler_main(void) {
    uint8_t status = inb(0x64);
    if ((status & 0x01) && (status & 0x20)) {
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
                mouse_x += (int8_t)mouse_byte[1];
                mouse_y -= (int8_t)mouse_byte[2];
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
