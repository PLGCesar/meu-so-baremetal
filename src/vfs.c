#include "../include/vfs.h"
#include "../include/ata.h"
#include "../include/serial.h"
#include "../include/util.h"
#include "../include/memory.h"

extern uint8_t foto_bmp_start[];
extern uint8_t foto_bmp_end[];
static vfs_entry_t file_table[MAX_FILES];

void vfs_init(void) {
    ata_init();
    ata_read_sector(0, (uint8_t*)file_table);

    if (file_table[0].is_used == 0) {
        serial_write("[VFS] HD Formatado. Multi-Setor Ativo!\n");
        const char* r = "BEM VINDO AO NOVO VFS DE ALTA CAPACIDADE!";
        vfs_write_file("readme.txt", (const uint8_t*)r, kstrlen(r));
    }
}

void vfs_list(char* out_buf, size_t max_len) {
    kmemset(out_buf, 0, max_len);
    size_t pos = 0;
    const char* f = "foto.bmp  ";
    while (*f) out_buf[pos++] = *f++;

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used) {
            size_t j = 0;
            while (file_table[i].name[j] && pos < max_len - 3) out_buf[pos++] = file_table[i].name[j++];
            out_buf[pos++] = ' '; out_buf[pos++] = ' ';
        }
    }
}

const uint8_t* vfs_read(const char* filename, size_t* out_size) {
    if (kstrcmp(filename, "foto.bmp") == 0) {
        *out_size = (size_t)(foto_bmp_end - foto_bmp_start);
        return foto_bmp_start;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && kstrcmp(file_table[i].name, filename) == 0) {
            *out_size = file_table[i].size;
            uint32_t num_sectors = (file_table[i].size + 511) / 512;
            uint8_t* buf = kmalloc(num_sectors * 512);
            for (uint32_t s = 0; s < num_sectors; s++) {
                ata_read_sector(file_table[i].offset + s, buf + (s * 512));
            }
            return buf;
        }
    }
    return NULL;
}

int vfs_write_file(const char* filename, const uint8_t* content, size_t size) {
    if (size > SECTORS_PER_FILE * 512) return 0; // Excede limite de 100KB

    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].is_used || kstrcmp(file_table[i].name, filename) == 0) {
            kstrcpy(file_table[i].name, filename);
            uint32_t start_sector = 1 + (i * SECTORS_PER_FILE);
            file_table[i].offset = start_sector;
            file_table[i].size = size;
            file_table[i].is_used = 1;

            uint32_t num_sectors = (size + 511) / 512;
            uint8_t buffer[512];

            for (uint32_t s = 0; s < num_sectors; s++) {
                kmemset(buffer, 0, 512);
                size_t to_copy = size - (s * 512);
                if (to_copy > 512) to_copy = 512;
                kmemcpy(buffer, content + (s * 512), to_copy);
                ata_write_sector(start_sector + s, buffer);
            }
            ata_write_sector(0, (uint8_t*)file_table);
            serial_write("[VFS] Arquivo Binario gravado com sucesso no HD!\n");
            return 1;
        }
    }
    return 0;
}
