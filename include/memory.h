#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

// Mantemos o alinhamento de 32-Bytes. Se for SSE2 ele desperdiça 16 bytes de padding, mas garante segurança.
#define ALIGNMENT_256 32

typedef struct block_header {
    uint64_t magic;
    size_t size;
    int is_free;
    uint32_t padding;
    struct block_header* next;
    struct block_header* prev;
    uint64_t pad_avx_1;
    uint64_t pad_avx_2;
    uint64_t pad_avx_3;
} __attribute__((aligned(32))) block_header_t;

void memory_init(multiboot_info_t* mbi);
void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

// --- API DINÂMICA DE ALTA PERFORMANCE ---
// Essas funções decidem internamente se usam AVX2 ou SSE2.
void kfast_memset_zero(void* dest, size_t bytes);
void kfast_memcpy(void* dest, const void* src, size_t bytes);
void kfast_memset_color(void* dest, uint32_t color, size_t bytes);

// --- PAGINAÇÃO AVANÇADA (VMM) ---
void vmm_init(void);
void vmm_map_page(uint64_t vaddr, uint64_t paddr, uint32_t flags);
void vmm_protect_page(uint64_t vaddr, uint32_t flags);

size_t memory_get_total_allocated(void);
size_t memory_get_total_free(void);
size_t memory_get_block_count(void);

#endif
