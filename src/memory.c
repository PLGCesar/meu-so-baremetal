#include "../include/memory.h"

extern uint32_t _kernel_end;
static block_header_t* heap_first = NULL;

static size_t total_allocated = 0;
static size_t total_free = 0;
static size_t block_count = 0;

void memory_init(multiboot_info_t* mbi) {
    (void)mbi;

    uintptr_t heap_start = (uintptr_t)&_kernel_end;
    if (heap_start % 8 != 0) {
        heap_start += 8 - (heap_start % 8);
    }

    size_t heap_size = 16 * 1024 * 1024; // 16 MB Heap

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
    if (size % 8 != 0) size += 8 - (size % 8);

    block_header_t* curr = heap_first;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
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

    return NULL;
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        uint8_t* p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->size >= new_size) return ptr;

    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        uint8_t* src = (uint8_t*)ptr;
        uint8_t* dst = (uint8_t*)new_ptr;
        for (size_t i = 0; i < block->size && i < new_size; i++) {
            dst[i] = src[i];
        }
        kfree(ptr);
    }
    return new_ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->is_free) return;

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
