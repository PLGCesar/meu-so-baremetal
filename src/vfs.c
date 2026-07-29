#include "../include/vfs.h"
#include "../include/serial.h"

extern uint8_t disk_start[];
extern uint8_t disk_end[];

static vfs_entry_t* file_table = NULL;
static uint8_t* disk_data = NULL;

static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void kstrcpy(char* dest, const char* src) {
    size_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void vfs_init(void) {
    disk_data = disk_start;
    file_table = (vfs_entry_t*)disk_data;
    serial_write("[VFS] Driver de Disco .img Inicializado com Sucesso!\n");
}

void vfs_list(char* out_buf, size_t max_len) {
    out_buf[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used) {
            size_t j = 0;
            while (file_table[i].name[j] != '\0' && pos < max_len - 3) {
                out_buf[pos++] = file_table[i].name[j++];
            }
            out_buf[pos++] = ' ';
            out_buf[pos++] = ' ';
        }
    }
    out_buf[pos] = '\0';
}

const char* vfs_read(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && kstrcmp(file_table[i].name, filename) == 0) {
            return (const char*)(disk_data + file_table[i].offset);
        }
    }
    return NULL;
}

int vfs_write_file(const char* filename, const char* content) {
    // 1. Procura se o arquivo já existe para sobrescrever
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && kstrcmp(file_table[i].name, filename) == 0) {
            uint8_t* ptr = disk_data + file_table[i].offset;
            size_t len = 0;
            while (content[len] != '\0' && len < 2040) {
                ptr[len] = content[len];
                len++;
            }
            ptr[len] = '\0';
            file_table[i].size = len;
            serial_write("[VFS] Arquivo sobrescrito no disco!\n");
            return 1;
        }
    }

    // 2. Cria um arquivo novo em um slot livre
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].is_used) {
            kstrcpy(file_table[i].name, filename);
            uint32_t offset = 1024 + (i * 2048);
            file_table[i].offset = offset;
            file_table[i].is_used = 1;

            uint8_t* ptr = disk_data + offset;
            size_t len = 0;
            while (content[len] != '\0' && len < 2040) {
                ptr[len] = content[len];
                len++;
            }
            ptr[len] = '\0';
            file_table[i].size = len;

            serial_write("[VFS] Novo arquivo gravado com sucesso no disco .img!\n");
            return 1;
        }
    }
    return 0; // Disco Cheio
}
