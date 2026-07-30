#include "../include/memory.h"
#include "../include/serial.h"

#define MAGIC_ALIVE 0xDEADBEEF

extern uint32_t _kernel_end;
static block_header_t* heap_first = NULL;
static size_t total_allocated = 0, total_free = 0, block_count = 0;

void memory_init(multiboot_info_t* mbi) {
    (void)mbi;
    uintptr_t heap_start = (uintptr_t)&_kernel_end;
    if (heap_start % 8 != 0) heap_start += 8 - (heap_start % 8);

    heap_first = (block_header_t*)heap_start;
    heap_first->size = (16 * 1024 * 1024) - sizeof(block_header_t);
    heap_first->is_free = 1;
    heap_first->next = NULL;
    
    total_free = heap_first->size;
    block_count = 1;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    if (size % 8 != 0) size += 8 - (size % 8);

    block_header_t* curr = heap_first;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size >= size + sizeof(block_header_t) + 32) {
                block_header_t* new_b = (block_header_t*)((uint8_t*)curr + sizeof(block_header_t) + size);
                new_b->size = curr->size - size - sizeof(block_header_t);
                new_b->is_free = 1;
                new_b->next = curr->next;
                curr->size = size;
                curr->next = new_b;
                block_count++;
            }
            curr->is_free = 0;
            total_allocated += curr->size;
            total_free -= curr->size;
            
            // Grava Magic Bound no final
            uint32_t* magic_bound = (uint32_t*)((uint8_t*)curr + sizeof(block_header_t) + curr->size - 4);
            *magic_bound = MAGIC_ALIVE;
            
            return (void*)((uint8_t*)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->is_free) return;

    // Checagem Anti-Overflow
    uint32_t* magic_bound = (uint32_t*)((uint8_t*)block + sizeof(block_header_t) + block->size - 4);
    if (*magic_bound != MAGIC_ALIVE) {
        serial_write("[PANIC] BUFFER OVERFLOW DETECTADO NA MEMORIA HEAP!\n");
    }

    block->is_free = 1;
    total_allocated -= block->size;
    total_free += block->size;

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
