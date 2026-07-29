#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

// --- PORTAS DE ENTRADA E SAÍDA (INB / OUTB) ---
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// --- TERMINAL VGA ---
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | ((uint16_t) color << 8);
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(15, 0); // Branco no Preto
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
        return;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = vga_entry(c, terminal_color);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    }
}

void terminal_write(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}

// Trata o Backspace (apagar caractere da tela)
void terminal_backspace(void) {
    if (terminal_column > 0) {
        terminal_column--;
    } else if (terminal_row > 0) {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    }
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = vga_entry(' ', terminal_color);
}

// --- kprintf() FORMATADO ---
void print_int(int value) {
    if (value < 0) { terminal_putchar('-'); value = -value; }
    if (value == 0) { terminal_putchar('0'); return; }
    char buf[32]; int i = 0;
    while (value > 0) { buf[i++] = '0' + (value % 10); value /= 10; }
    while (--i >= 0) { terminal_putchar(buf[i]); }
}

void print_hex(uint32_t value) {
    terminal_write("0x");
    if (value == 0) { terminal_putchar('0'); return; }
    char buf[32]; int i = 0;
    const char* hex_chars = "0123456789ABCDEF";
    while (value > 0) { buf[i++] = hex_chars[value % 16]; value /= 16; }
    while (--i >= 0) { terminal_putchar(buf[i]); }
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 'c': terminal_putchar((char) va_arg(args, int)); break;
                case 's': terminal_write(va_arg(args, const char*)); break;
                case 'd': print_int(va_arg(args, int)); break;
                case 'x': case 'p': print_hex(va_arg(args, uint32_t)); break;
                case '%': terminal_putchar('%'); break;
            }
        } else {
            terminal_putchar(fmt[i]);
        }
    }
    va_end(args);
}

// --- GERENCIADOR DE MEMÓRIA (BUMP ALLOCATOR) ---
extern uint32_t _kernel_end;
static uintptr_t heap_curr = 0;

void memory_init(void) {
    heap_curr = (uintptr_t)&_kernel_end;
}

void* kmalloc(size_t size) {
    if (size % 4 != 0) size += 4 - (size % 4);
    void* ptr = (void*)heap_curr;
    heap_curr += size;
    return ptr;
}

// --- ESTRUTURAS DA IDT E PIC ---
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
extern void irq1_stub(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28); // Mapeia IRQ0-15 para interrupções 32-47
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x00); outb(0xA1, 0x00);
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    pic_remap();

    // Registra a interrupção 33 (IRQ1 = Teclado PS/2)
    idt_set_gate(33, (uint32_t)irq1_stub, 0x08, 0x8E);

    load_idt(&idtp);
}

// --- DRIVER DO TECLADO E MINI-SHELL ---
static char shell_buffer[256];
static size_t shell_index = 0;

int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void process_command(void) {
    shell_buffer[shell_index] = '\0';
    kprintf("\n");

    if (kstrcmp(shell_buffer, "help") == 0) {
        kprintf("Comandos disponiveis:\n");
        kprintf("  help   - Mostra esta lista de ajuda\n");
        kprintf("  clear  - Limpa a tela do terminal\n");
        kprintf("  fetch  - Mostra informacoes do sistema\n");
        kprintf("  alloc  - Testa a alocacao dinamica de memoria\n");
    } else if (kstrcmp(shell_buffer, "clear") == 0) {
        terminal_initialize();
    } else if (kstrcmp(shell_buffer, "fetch") == 0) {
        kprintf("  _______   MEU SO BARE-METAL v0.3\n");
        kprintf(" |  ___  |  Kernel: 32-bit Protected Mode\n");
        kprintf(" | |   | |  Teclado: Driver PS/2 Ativo\n");
        kprintf(" |_|   |_|  Heap Atual: %x\n", heap_curr);
    } else if (kstrcmp(shell_buffer, "alloc") == 0) {
        void* ptr = kmalloc(64);
        kprintf("[KMALLOC] Alocados 64 bytes no endereco: %p\n", ptr);
    } else if (shell_index > 0) {
        kprintf("Comando desconhecido: '%s'. Digite 'help'.\n", shell_buffer);
    }

    shell_index = 0;
    kprintf("myos> ");
}

// Tabela de conversão de Scancode PS/2 para Letras ASCII
const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

// Função chamada quando a CPU recebe a interrupção do Teclado (IRQ1)
void keyboard_handler_main(void) {
    uint8_t scancode = inb(0x60); // Lê o sinal vindo do chip do teclado

    // Avisa o PIC que a interrupção foi processada (EOI)
    outb(0x20, 0x20);

    // Se o bit 7 for 0, significa tecla PRESSIONADA (não solta)
    if (!(scancode & 0x80)) {
        char c = scancode_ascii[scancode];
        if (c == '\n') {
            process_command();
        } else if (c == '\b') {
            if (shell_index > 0) {
                shell_index--;
                terminal_backspace();
            }
        } else if (c != 0) {
            if (shell_index < 255) {
                shell_buffer[shell_index++] = c;
                terminal_putchar(c);
            }
        }
    }
}

// --- PONTO DE ENTRADA DO KERNEL ---
void kernel_main(void) {
    terminal_initialize();
    memory_init();
    idt_init(); // Inicializa as Interrupções e o Teclado!

    kprintf("=== SISTEMA OPERACIONAL BARE-METAL (v0.3) ===\n");
    kprintf("Teclado PS/2 e Tabela IDT Carregados com Sucesso!\n\n");
    kprintf("Digite 'help' para ver os comandos.\n\n");
    kprintf("myos> ");

    // Loop infinito mantendo a CPU viva aguardando interrupções
    while (1) {
        asm volatile ("hlt");
    }
}
