#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FILES 16

typedef struct __attribute__((packed)) {
    char name[32];      // Nome do arquivo
    uint32_t size;      // Tamanho em bytes
    uint32_t offset;    // Endereço dentro do disk.img
    uint8_t  is_used;   // 1 = Ativo, 0 = Livre
    uint8_t  reserved[23];
} vfs_entry_t;

void vfs_init(void);
void vfs_list(char* out_buf, size_t max_len);
const char* vfs_read(const char* filename);
int vfs_write_file(const char* filename, const char* content);

#endif
