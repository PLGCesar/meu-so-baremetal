#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

#define ALIGNMENT_128 16

typedef struct block_header {
    uint64_t magic;
    size_t size;
    int is_free;
    uint32_t padding;
    struct block_header* next;
    struct block_header* prev;
} __attribute__((aligned(16))) block_header_t;

void memory_init(multiboot_info_t* mbi);
void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

// API SIMD SSE2 128-BITS
void kmemset128_zero(void* dest, size_t count_16);
void kmemcpy128(void* dest, const void* src, size_t count_16);
void kmemset128_color(void* dest, uint32_t color, size_t count_16);

// VMM
void vmm_init(void);
void vmm_map_page(uint64_t vaddr, uint64_t paddr, uint32_t flags);
void vmm_protect_page(uint64_t vaddr, uint32_t flags);

size_t memory_get_total_allocated(void);
size_t memory_get_total_free(void);
size_t memory_get_block_count(void);

#endif
