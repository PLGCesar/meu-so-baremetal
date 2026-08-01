#include "../include/memory.h"
#include "../include/serial.h"

#define MAGIC_HEADER 0xDEADBEEFCAFEBABEULL
#define MAGIC_FOOTER 0xCAFEBABEDEADBEEFULL

extern uint32_t _kernel_end;
static block_header_t* heap_first = NULL;
static size_t total_allocated = 0;
static size_t total_free = 0;
static size_t block_count = 0;

// Otimizador de preenchimento de memória de 64-bits (move 8 bytes por ciclo de CPU)
void kmemset64(void* dest, uint64_t val, size_t count_64) {
    uint64_t* ptr = (uint64_t*)dest;
    while (count_64--) {
        *ptr++ = val;
    }
}

// Otimizador de cópia de memória de 64-bits (move 8 bytes por ciclo de CPU)
void kmemcpy64(void* dest, const void* src, size_t count_64) {
    uint64_t* d = (uint64_t*)dest;
    const uint64_t* s = (const uint64_t*)src;
    while (count_64--) {
        *d++ = *s++;
    }
}

void memory_init(multiboot_info_t* mbi) {
    (void)mbi;
    uintptr_t heap_start = (uintptr_t)&_kernel_end;
    
    // Alinhamento rigoroso de 16-bytes para a ABI de 64-bits (x86_64)
    if (heap_start % ALIGNMENT_64 != 0) {
        heap_start += ALIGNMENT_64 - (heap_start % ALIGNMENT_64);
    }

    size_t heap_size = 32 * 1024 * 1024; // 32 MegaBytes de Heap inicial em 64-bits!

    heap_first = (block_header_t*)heap_start;
    heap_first->magic = MAGIC_HEADER;
    heap_first->size = heap_size - sizeof(block_header_t) - sizeof(uint64_t);
    heap_first->is_free = 1;
    heap_first->next = NULL;
    heap_first->prev = NULL;

    // Grava o footer mágico no final do primeiro bloco
    uint64_t* footer = (uint64_t*)((uint8_t*)heap_first + sizeof(block_header_t) + heap_first->size);
    *footer = MAGIC_FOOTER;

    total_free = heap_first->size;
    total_allocated = 0;
    block_count = 1;

    serial_write("[64-BIT MEMORY] Heap 64-bit 16-Byte Aligned Inicializado com 32MB!\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Alinha o tamanho para múltiplos de 16-bytes
    if (size % ALIGNMENT_64 != 0) {
        size += ALIGNMENT_64 - (size % ALIGNMENT_64);
    }

    block_header_t* curr = heap_first;

    while (curr) {
        // Checagem de integridade do cabeçalho 64-bit
        if (curr->magic != MAGIC_HEADER) {
            serial_write("[KERNEL PANIC] CORRUPCAO DE HEAP DETECTADA EM 64-BITS!\n");
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            // Divisão de bloco (Splitting) se sobrar espaço suficiente
            if (curr->size >= size + sizeof(block_header_t) + sizeof(uint64_t) + 32) {
                size_t remaining_size = curr->size - size - sizeof(block_header_t) - sizeof(uint64_t);

                block_header_t* new_b = (block_header_t*)((uint8_t*)curr + sizeof(block_header_t) + size + sizeof(uint64_t));
                new_b->magic = MAGIC_HEADER;
                new_b->size = remaining_size;
                new_b->is_free = 1;
                new_b->next = curr->next;
                new_b->prev = curr;

                if (curr->next) curr->next->prev = new_b;
                curr->next = new_b;

                uint64_t* new_footer = (uint64_t*)((uint8_t*)new_b + sizeof(block_header_t) + new_b->size);
                *new_footer = MAGIC_FOOTER;

                curr->size = size;
                block_count++;
            }

            curr->is_free = 0;
            total_allocated += curr->size;
            total_free -= curr->size;

            uint64_t* footer = (uint64_t*)((uint8_t*)curr + sizeof(block_header_t) + curr->size);
            *footer = MAGIC_FOOTER;

            return (void*)((uint8_t*)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }

    return NULL;
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        // Preenchimento veloz de 64-bits por quadwords (8 bytes por ciclo)
        size_t qwords = (total + 7) / 8;
        kmemset64(ptr, 0, qwords);
    }
    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->magic != MAGIC_HEADER) {
        serial_write("[PANIC] PONTEIRO INVALIDO EM KREALLOC!\n");
        return NULL;
    }

    if (block->size >= new_size) return ptr;

    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        size_t qwords = (block->size + 7) / 8;
        kmemcpy64(new_ptr, ptr, qwords);
        kfree(ptr);
    }
    return new_ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->magic != MAGIC_HEADER) {
        serial_write("[PANIC] CORRUPCAO DE CABECALHO AO LIBERAR MEMORIA (KFREE)!\n");
        return;
    }

    uint64_t* footer = (uint64_t*)((uint8_t*)block + sizeof(block_header_t) + block->size);
    if (*footer != MAGIC_FOOTER) {
        serial_write("[PANIC] OVERFLOW DE BLOCO DETECTADO AO LIBERAR MEMORIA (KFREE)!\n");
    }

    if (block->is_free) return;

    block->is_free = 1;
    total_allocated -= block->size;
    total_free += block->size;

    // FUSÃO RÁPIDA O(1) DE BLOCOS LIVRES VIZINHOS (Coalescing Lista Duplamente Encadeada)
    if (block->next && block->next->is_free) {
        block->size += sizeof(block_header_t) + sizeof(uint64_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
        block_count--;
    }

    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(block_header_t) + sizeof(uint64_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
        block_count--;
    }
}

size_t memory_get_total_allocated(void) { return total_allocated; }
size_t memory_get_total_free(void) { return total_free; }
size_t memory_get_block_count(void) { return block_count; }
