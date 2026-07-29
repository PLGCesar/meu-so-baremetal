#include "../include/memory.h"

extern uint32_t _kernel_end;
static block_header_t* heap_first = NULL;

static size_t total_allocated = 0;
static size_t total_free = 0;
static size_t block_count = 0;

void memory_init(multiboot_info_t* mbi) {
    (void)mbi;

    // Inicia o Heap alinhado logo após o fim do Kernel
    uintptr_t heap_start = (uintptr_t)&_kernel_end;
    if (heap_start % 8 != 0) {
        heap_start += 8 - (heap_start % 8); // Alinhamento de 8 bytes
    }

    size_t heap_size = 16 * 1024 * 1024; // 16 Megabytes de Heap Inicial

    heap_first = (block_header_t*)heap_start;
    heap_first->size = heap_size - sizeof(block_header_t);
    heap_first->is_free = 1;
    heap_first->next = NULL;

    total_free = heap_first->size;
    total_allocated = 0;
    block_count = 1;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Alinha o tamanho solicitado em múltiplos de 8 bytes
    if (size % 8 != 0) {
        size += 8 - (size % 8);
    }

    block_header_t* curr = heap_first;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // DIVISÃO DE BLOCO (Splitting) se sobrar espaço útil
            if (curr->size >= size + sizeof(block_header_t) + 32) {
                block_header_t* new_block = (block_header_t*)((uint8_t*)curr + sizeof(block_header_t) + size);
                new_block->size = curr->size - size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
                block_count++;
            }

            curr->is_free = 0;
            total_allocated += curr->size;
            total_free -= curr->size;

            return (void*)((uint8_t*)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }

    return NULL; // Sem memória suficiente
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->is_free) return; // Evita double free

    block->is_free = 1;
    total_allocated -= block->size;
    total_free += block->size;

    // FUSÃO DE BLOCOS LIVRES (Coalescing) para eliminar fragmentação
    block_header_t* curr = heap_first;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
            block_count--;
        } else {
            curr = curr->next;
        }
    }
}

size_t memory_get_total_allocated(void) { return total_allocated; }
size_t memory_get_total_free(void) { return total_free; }
size_t memory_get_block_count(void) { return block_count; }
