#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

typedef struct block_header {
    size_t size;                // Tamanho do bloco de dados
    int is_free;                // 1 = Livre, 0 = Ocupado
    struct block_header* next;  // Próximo bloco no Heap
} block_header_t;

void memory_init(multiboot_info_t* mbi);
void* kmalloc(size_t size);
void kfree(void* ptr);

size_t memory_get_total_allocated(void);
size_t memory_get_total_free(void);
size_t memory_get_block_count(void);

#endif
