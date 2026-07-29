#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

// --- ESTADO DO TERMINAL ---
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
    terminal_color = vga_entry_color(15, 0); // Texto Branco (15), Fundo Preto (0)
    
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
    }
}

// Rolagem de tela quando chega na linha 25
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

// --- FUNÇÕES AUXILIARES DE CONVERSÃO ---
void print_int(int value) {
    if (value < 0) {
        terminal_putchar('-');
        value = -value;
    }
    if (value == 0) {
        terminal_putchar('0');
        return;
    }
    char buf[32];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (--i >= 0) {
        terminal_putchar(buf[i]);
    }
}

void print_hex(uint32_t value) {
    terminal_write("0x");
    if (value == 0) {
        terminal_putchar('0');
        return;
    }
    char buf[32];
    int i = 0;
    const char* hex_chars = "0123456789ABCDEF";
    while (value > 0) {
        buf[i++] = hex_chars[value % 16];
        value /= 16;
    }
    while (--i >= 0) {
        terminal_putchar(buf[i]);
    }
}

// --- kprintf() NOSSA IMPLEMENTAÇÃO ---
void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 'c':
                    terminal_putchar((char) va_arg(args, int));
                    break;
                case 's':
                    terminal_write(va_arg(args, const char*));
                    break;
                case 'd':
                    print_int(va_arg(args, int));
                    break;
                case 'x':
                case 'p':
                    print_hex(va_arg(args, uint32_t));
                    break;
                case '%':
                    terminal_putchar('%');
                    break;
            }
        } else {
            terminal_putchar(fmt[i]);
        }
    }
    va_end(args);
}

// --- GERENCIADOR DE MEMÓRIA (BUMP ALLOCATOR) ---
extern uint32_t _kernel_end; // Marcador vindo do linker.ld
static uintptr_t heap_curr = 0;

void memory_init(void) {
    heap_curr = (uintptr_t)&_kernel_end;
}

// Função de alocação de memória dinâmica do Kernel
void* kmalloc(size_t size) {
    // Alinha a memória em múltiplos de 4 bytes
    if (size % 4 != 0) {
        size += 4 - (size % 4);
    }
    void* ptr = (void*)heap_curr;
    heap_curr += size;
    return ptr;
}

// --- PONTO DE ENTRADA DO KERNEL ---
void kernel_main(void) {
    terminal_initialize();
    memory_init();

    kprintf("=== SISTEMA OPERACIONAL BARE-METAL ===\n");
    kprintf("Status: Kernel carregado e inicializado!\n\n");

    kprintf("[MEMORIA] Fim do Kernel / Inicio do Heap em: %x\n\n", (uint32_t)&_kernel_end);

    // TESTE DE ALOCAÇÃO DINÂMICA DE MEMÓRIA (kmalloc)
    kprintf("[KMALLOC] Testando alocacao dinamica...\n");

    int* array = (int*) kmalloc(5 * sizeof(int));
    char* texto = (char*) kmalloc(64);

    kprintf(" -> Bloco 1 (Array 5 ints) alocado no endereco: %p\n", array);
    kprintf(" -> Bloco 2 (String 64 bytes) alocado no endereco: %p\n\n", texto);

    // Preenchendo a memória alocada dinamicamente
    kprintf("[TESTE] Gravando dados no Array dinamico:\n");
    for (int i = 0; i < 5; i++) {
        array[i] = (i + 1) * 100;
        kprintf("    array[%d] = %d\n", i, array[i]);
    }

    kprintf("\n[MEMORIA] Proximo endereco livre no Heap: %x\n", heap_curr);
    kprintf("\n[STATUS] Kernel pronto para o proximo passo!\n");
}
