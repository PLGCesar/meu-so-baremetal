#include "../include/vfs.h"
#include "../include/ata.h"
#include "../include/serial.h"

extern uint8_t foto_bmp_start[];

static vfs_entry_t file_table[MAX_FILES];
static uint8_t sector_buffer[512];

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
    ata_init();

    // Lê o Setor 0 do HD (onde fica o cabeçalho dos arquivos)
    ata_read_sector(0, (uint8_t*)file_table);

    // Se o HD estiver novo/zerado, formata e grava os arquivos padrão no HD!
    if (file_table[0].is_used == 0) {
        serial_write("[VFS] HD Novo detectado! Formatando e gravando arquivos padrao no HD...\n");
        vfs_write_file("readme.txt", "BEM VINDO AO HD FISICO GRAVADO VIA ATA IDE!");
        vfs_write_file("notas.txt", "TUDO O QUE VOCE GRAVAR AQUI PERSISTE NO HD!");
    } else {
        serial_write("[VFS] HD com dados persistentes lido do setor 0!\n");
    }
}

void vfs_list(char* out_buf, size_t max_len) {
    out_buf[0] = '\0';
    size_t pos = 0;

    const char* foto_name = "foto.bmp  ";
    for (int k = 0; foto_name[k] != '\0'; k++) out_buf[pos++] = foto_name[k];

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
    if (kstrcmp(filename, "foto.bmp") == 0) {
        return (const char*)foto_bmp_start;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && kstrcmp(file_table[i].name, filename) == 0) {
            // Lê o setor físico correspondente do HD
            ata_read_sector(file_table[i].offset, sector_buffer);
            return (const char*)sector_buffer;
        }
    }
    return NULL;
}

int vfs_write_file(const char* filename, const char* content) {
    // 1. Sobrescreve arquivo existente no HD
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && kstrcmp(file_table[i].name, filename) == 0) {
            uint8_t buf[512] = {0};
            size_t len = 0;
            while (content[len] != '\0' && len < 510) {
                buf[len] = content[len];
                len++;
            }
            buf[len] = '\0';

            // Grava o setor de dados no HD
            ata_write_sector(file_table[i].offset, buf);

            // Atualiza a Tabela de Arquivos no Setor 0 do HD
            file_table[i].size = len;
            ata_write_sector(0, (uint8_t*)file_table);
            return 1;
        }
    }

    // 2. Cria novo arquivo em um novo setor do HD (Setores 1, 2, 3...)
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].is_used) {
            kstrcpy(file_table[i].name, filename);
            uint32_t sector_lba = i + 1; // Setores 1 a 16 do HD
            file_table[i].offset = sector_lba;
            file_table[i].is_used = 1;

            uint8_t buf[512] = {0};
            size_t len = 0;
            while (content[len] != '\0' && len < 510) {
                buf[len] = content[len];
                len++;
            }
            buf[len] = '\0';
            file_table[i].size = len;

            // Grava o novo setor de dados no HD
            ata_write_sector(sector_lba, buf);

            // Grava a Tabela de Arquivos atualizada no Setor 0 do HD
            ata_write_sector(0, (uint8_t*)file_table);
            return 1;
        }
    }
    return 0;
}
