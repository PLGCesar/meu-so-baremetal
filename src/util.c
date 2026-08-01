#include "../include/util.h"
#include "../include/klibc.h"
#include "../include/memory.h"

// Wrappers mantidos para compatibilidade com codigo antigo sem quebrar nada
void kmemset(void* dest, uint8_t val, size_t count) { fast_memset(dest, val, count); }
void kmemcpy(void* dest, const void* src, size_t count) { fast_memcpy(dest, src, count); }

kvector_t* kvector_create(size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 4;
    kvector_t* vec = (kvector_t*)kmalloc(sizeof(kvector_t));
    if (!vec) return NULL;
    vec->data = (uint64_t*)kmalloc(initial_cap * sizeof(uint64_t));
    vec->capacity = initial_cap;
    vec->size = 0;
    return vec;
}

void kvector_push(kvector_t* vec, uint64_t val) {
    if (!vec) return;
    if (vec->size >= vec->capacity) {
        size_t new_cap = vec->capacity * 2;
        vec->data = (uint64_t*)krealloc(vec->data, new_cap * sizeof(uint64_t));
        vec->capacity = new_cap;
    }
    vec->data[vec->size++] = val;
}

uint64_t kvector_get(kvector_t* vec, size_t index) {
    if (!vec || index >= vec->size) return 0;
    return vec->data[index];
}

void kvector_free(kvector_t* vec) {
    if (!vec) return;
    if (vec->data) kfree(vec->data);
    kfree(vec);
}
