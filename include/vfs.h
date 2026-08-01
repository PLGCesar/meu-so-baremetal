#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FILES 8
#define SECTORS_PER_FILE 200

typedef struct __attribute__((packed)) {
    char     name[32];
    uint64_t size;    // ESTRUTURA DE 64-BITS (SUPORTA EXABYTES)
    uint64_t offset;  // ESTRUTURA DE 64-BITS
    uint8_t  is_used;
    uint8_t  reserved[15];
} vfs_entry_t;

void vfs_init(void);
void vfs_list(char* out_buf, size_t max_len);
void vfs_list_custom_format(char* out_buf, size_t max_len);
const uint8_t* vfs_read(const char* filename, size_t* out_size);
int vfs_write_file(const char* filename, const uint8_t* content, size_t size);

#endif
