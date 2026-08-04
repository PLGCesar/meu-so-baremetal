#include "../include/memory.h"
#include "../include/serial.h"

#define MAGIC_HEADER 0xDEADBEEFCAFEBABEULL
#define MAGIC_FOOTER 0xCAFEBABEDEADBEEFULL

extern uint32_t _kernel_end;
static block_header_t* heap_first = NULL;
static size_t total_allocated = 0;
static size_t total_free = 0;
static size_t block_count = 0;

// SIMD SSE2: ZERA BLOCOS DE 16 BYTES USANDO REGISTRADORES XMM DE 128-BITS (PXOR)
void kmemset128_zero(void* dest, size_t count_16) {
    if (!dest || count_16 == 0) return;
    __asm__ volatile (
        "pxor %%xmm0, %%xmm0\n\t"
        "1:\n\t"
        "movdqu %%xmm0, (%0)\n\t"
        "add $16, %0\n\t"
        "dec %1\n\t"
        "jnz 1b"
        : "+r"(dest), "+r"(count_16)
        :
        : "xmm0", "memory"
    );
}

// SIMD SSE2: COPIA BLOCOS DE 16 BYTES USANDO REGISTRADORES XMM DE 128-BITS
void kmemcpy128(void* dest, const void* src, size_t count_16) {
    if (!dest || !src || count_16 == 0) return;
    __asm__ volatile (
        "1:\n\t"
        "movdqu (%1), %%xmm0\n\t"
        "movdqu %%xmm0, (%0)\n\t"
        "add $16, %0\n\t"
        "add $16, %1\n\t"
        "dec %2\n\t"
        "jnz 1b"
        : "+r"(dest), "+r"(src), "+r"(count_16)
        :
        : "xmm0", "memory"
    );
}

void kmemset64(void* dest, uint64_t val, size_t count_64) {
    uint64_t* ptr = (uint64_t*)dest;
    while (count_64--) { *ptr++ = val; }
}

void kmemcpy64(void* dest, const void* src, size_t count_64) {
    uint64_t* d = (uint64_t*)dest;
    const uint64_t* s = (const uint64_t*)src;
    while (count_64--) { *d++ = *s++; }
}

void memory_init(multiboot_info_t* mbi) {
    (void)mbi;
    uintptr_t heap_start = (uintptr_t)&_kernel_end;
    
    if (heap_start % ALIGNMENT_64 != 0) {
        heap_start += ALIGNMENT_64 - (heap_start % ALIGNMENT_64);
    }

    size_t heap_size = 32 * 1024 * 1024; // 32 MB de Heap Inicial

    heap_first = (block_header_t*)heap_start;
    heap_first->magic = MAGIC_HEADER;
    heap_first->size = heap_size - sizeof(block_header_t) - sizeof(uint64_t);
    heap_first->is_free = 1;
    heap_first->next = NULL;
    heap_first->prev = NULL;

    uint64_t* footer = (uint64_t*)((uint8_t*)heap_first + sizeof(block_header_t) + heap_first->size);
    *footer = MAGIC_FOOTER;

    total_free = heap_first->size;
    total_allocated = 0;
    block_count = 1;

    serial_write("[64-BIT MEMORY] Heap 32MB com SIMD SSE2 128-Bits Alinhado Inicializado!\n");
}

void* kmalloc(size_t size) {
    if (size == 0 || size > 1024 * 1024 * 128) return NULL;

    // Alinha tamanho util a multiplos de 16 bytes
    if (size % ALIGNMENT_64 != 0) {
        size += ALIGNMENT_64 - (size % ALIGNMENT_64);
    }

    block_header_t* curr = heap_first;

    while (curr) {
        if (curr->magic != MAGIC_HEADER) {
            serial_write("[KERNEL PANIC] CORRUPCAO DE HEAP DETECTADA!\n");
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
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
        // OTIMIZACAO SIMD SSE2: Zera blocos de 16-bytes usando registradores XMM de 128-bits (PXOR)
        size_t blocks_16 = total >> 4;
        size_t remainder = total & 15;
        if (blocks_16 > 0) {
            kmemset128_zero(ptr, blocks_16);
        }
        if (remainder > 0) {
            uint8_t* p = (uint8_t*)ptr + (blocks_16 << 4);
            while (remainder--) *p++ = 0;
        }
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
        size_t blocks_16 = block->size >> 4;
        size_t remainder = block->size & 15;
        if (blocks_16 > 0) {
            kmemcpy128(new_ptr, ptr, blocks_16);
        }
        if (remainder > 0) {
            uint8_t* d = (uint8_t*)new_ptr + (blocks_16 << 4);
            const uint8_t* s = (const uint8_t*)ptr + (blocks_16 << 4);
            while (remainder--) *d++ = *s++;
        }
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

// --- PAGINAÇÃO AVANÇADA DE 4 NÍVEIS (PML4) E PROTEÇÃO DE MEMÓRIA (VMM) ---
#define PAGE_PRESENT  0x01
#define PAGE_WRITABLE 0x02
#define PAGE_USER     0x04

static uint64_t* kernel_pml4 = NULL;

void vmm_init(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (uint64_t*)(cr3 & 0xFFFFFFFFFFFFF000ULL);
    serial_write("[VMM] Sistema de Paginação 4-Níveis Inicializado p/ Proteção!\n");
}

void vmm_map_page(uint64_t vaddr, uint64_t paddr, uint32_t flags) {
    if (!kernel_pml4) return;
    
    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdp_idx  = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx   = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx   = (vaddr >> 12) & 0x1FF;

    if (!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) {
        uint64_t* new_pdp = (uint64_t*)kcalloc(512, sizeof(uint64_t));
        kernel_pml4[pml4_idx] = (uint64_t)new_pdp | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    uint64_t* pdp = (uint64_t*)(kernel_pml4[pml4_idx] & 0xFFFFFFFFFFFFF000ULL);
    if (!(pdp[pdp_idx] & PAGE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)kcalloc(512, sizeof(uint64_t));
        pdp[pdp_idx] = (uint64_t)new_pd | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    uint64_t* pd = (uint64_t*)(pdp[pdp_idx] & 0xFFFFFFFFFFFFF000ULL);
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        uint64_t* new_pt = (uint64_t*)kcalloc(512, sizeof(uint64_t));
        pd[pd_idx] = (uint64_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & 0xFFFFFFFFFFFFF000ULL);
    pt[pt_idx] = (paddr & 0xFFFFFFFFFFFFF000ULL) | flags;
    
    asm volatile("invlpg (%0)" ::"r" (vaddr) : "memory");
}

void vmm_protect_page(uint64_t vaddr, uint32_t flags) {
    vmm_map_page(vaddr, vaddr, flags); // Altera permissões dinâmicas
}
