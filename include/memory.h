#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

// Alinhamento rigoroso de 16-bytes exigido pela ABI x86_64
#define ALIGNMENT_64 16

typedef struct block_header {
    uint64_t magic;             // Assinatura Mágica de 64-bits
    size_t size;                // Tamanho útil da carga alocada
    int is_free;                // 1 = Livre, 0 = Ocupado
    struct block_header* next;  // Próximo bloco no Heap 64-bit
    struct block_header* prev;  // Bloco anterior (Lista Duplamente Encadeada)
} block_header_t;

void memory_init(multiboot_info_t* mbi);
void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

// Funções de alta velocidade em 64-bits (movem 8 bytes por ciclo)
void kmemset64(void* dest, uint64_t val, size_t count_64);
void kmemcpy64(void* dest, const void* src, size_t count_64);

size_t memory_get_total_allocated(void);
size_t memory_get_total_free(void);
size_t memory_get_block_count(void);

#endif
