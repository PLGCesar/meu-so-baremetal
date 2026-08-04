#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

#define ALIGNMENT_64 16

typedef struct block_header {
    uint64_t magic;             // Assinatura Magica de 64-bits
    size_t size;                // Tamanho util do bloco
    int is_free;                // 1 = Livre, 0 = Ocupado
    uint32_t padding;           // Alinhamento rigoroso a 16-bytes
    struct block_header* next;  // Proximo bloco no Heap
    struct block_header* prev;  // Bloco anterior
} __attribute__((aligned(16))) block_header_t;

void memory_init(multiboot_info_t* mbi);
void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

// FUNCOES SIMD SSE2 XMM 128-BITS (TRANSFEREM 16 BYTES POR CICLO DE CPU)
void kmemset128_zero(void* dest, size_t count_16);
void kmemcpy128(void* dest, const void* src, size_t count_16);
void kmemset64(void* dest, uint64_t val, size_t count_64);
void kmemcpy64(void* dest, const void* src, size_t count_64);

size_t memory_get_total_allocated(void);
size_t memory_get_total_free(void);
size_t memory_get_block_count(void);

#endif

// --- PAGINAÇÃO AVANÇADA E PROTEÇÃO DE MEMÓRIA ---
void vmm_init(void);
void vmm_map_page(uint64_t vaddr, uint64_t paddr, uint32_t flags);
void vmm_protect_page(uint64_t vaddr, uint32_t flags);
