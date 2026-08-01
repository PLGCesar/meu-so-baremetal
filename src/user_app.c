// Programa Ring 3 executado em Espaco de Usuario Isolado do CapivaraOS
typedef unsigned long uint64_t;

// Wrapper de Syscall de 64-bits que conversa com o Kernel via instrucao 'syscall'
static inline uint64_t sys_call(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    asm volatile (
        "mov %1, %%rax\n\t"
        "mov %2, %%rdi\n\t"
        "mov %3, %%rsi\n\t"
        "mov %4, %%rdx\n\t"
        "syscall"
        : "=a"(ret)
        : "r"(sys_num), "r"(arg1), "r"(arg2), "r"(arg3)
        : "rcx", "r11", "rdi", "rsi", "rdx", "memory"
    );
    return ret;
}

// Ponto de Entrada do ELF64 em Ring 3
void _start(void) {
    // Toca notas musicais via Syscall 3 (sound_play) e Syscall 4 (sound_stop)
    sys_call(3, 523, 0, 0); // Do
    for (volatile int i = 0; i < 8000000; i++);
    
    sys_call(3, 659, 0, 0); // Mi
    for (volatile int i = 0; i < 8000000; i++);
    
    sys_call(3, 784, 0, 0); // Sol
    for (volatile int i = 0; i < 8000000; i++);
    
    sys_call(4, 0, 0, 0);   // Para o som

    // Loop de Processo Ring 3 Ativo em segundo plano
    while (1) {
        asm volatile ("nop");
    }
}
