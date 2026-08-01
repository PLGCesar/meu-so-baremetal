#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

#define ALIGNMENT_64 16

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

void kmemset64(void* dest, uint64_t val, size_t count_64);
void kmemcpy64(void* dest, const void* src, size_t count_64);

size_t memory_get_total_allocated(void);
size_t memory_get_total_free(void);
size_t memory_get_block_count(void);

#endif
