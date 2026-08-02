#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../include/vfs.h"
#include "../include/fat32.h"
#include "../include/serial.h"
#include "../include/util.h"
#include "../include/bmp_assets.h"

extern uint8_t foto_bmp_start[];
extern uint8_t foto_bmp_end[];
extern uint8_t bgif_anim_start[];
extern uint8_t bgif_anim_end[];
extern uint8_t app_elf_start[];
extern uint8_t app_elf_end[];

void vfs_init(void) {
    fat32_init();
    serial_write("[VFS] VFS Conectado com Suporte Completo a foto.bmp, animacao.bgif e FAT32!\n");
}

void vfs_list(char* out_buf, size_t max_len) {
    (void)max_len;
    out_buf[0] = '\0';
    kstrcpy(out_buf, "app.elf  animacao.bgif  foto.bmp  icons.cfg  ");
    for (size_t i = 0; i < g_embedded_assets_count; i++) {
        size_t len = kstrlen(out_buf);
        if (len + kstrlen(g_embedded_assets[i].name) + 2 < max_len) {
            kstrcpy(out_buf + len, g_embedded_assets[i].name);
            len = kstrlen(out_buf);
            out_buf[len] = ' '; out_buf[len+1] = ' '; out_buf[len+2] = '\0';
        }
    }
}

void vfs_list_custom_format(char* out_buf, size_t max_len) {
    (void)max_len;
    out_buf[0] = '\0';
    kstrcpy(out_buf, "#|ROOT*app.elf\n#|ROOT*animacao.bgif\n#|ROOT*foto.bmp\n#|ROOT*icons.cfg\n");
    for (size_t i = 0; i < g_embedded_assets_count; i++) {
        size_t len = kstrlen(out_buf);
        kstrcpy(out_buf + len, "#|ROOT*");
        kstrcpy(out_buf + kstrlen(out_buf), g_embedded_assets[i].name);
        kstrcpy(out_buf + kstrlen(out_buf), "\n");
    }
}

const uint8_t* vfs_read(const char* filename, size_t* out_size) {
    if (!filename) return NULL;
    const char* clean_name = filename;
    if (kstrncmp(filename, "#|ROOT*", 7) == 0) {
        clean_name = filename + 7;
    }

    // 1. Arquivos binarios nativos embutidos no kernel (boot.s)
    if (kstrcmp(clean_name, "app.elf") == 0) {
        *out_size = (size_t)(app_elf_end - app_elf_start);
        return app_elf_start;
    }
    if (kstrcmp(clean_name, "animacao.bgif") == 0) {
        *out_size = (size_t)(bgif_anim_end - bgif_anim_start);
        return bgif_anim_start;
    }
    if (kstrcmp(clean_name, "foto.bmp") == 0) {
        // Tenta ler do disco FAT32 se foi salvo/sobrescrito
        const uint8_t* disk_data = fat32_read_file(clean_name, out_size);
        if (disk_data) return disk_data;

        size_t sz = (size_t)(foto_bmp_end - foto_bmp_start);
        if (sz > 0) {
            *out_size = sz;
            return foto_bmp_start;
        }
    }

    // 2. Tenta ler do disco FAT32
    const uint8_t* disk_data = fat32_read_file(clean_name, out_size);
    if (disk_data) return disk_data;

    // 3. Tenta encontrar no banco de assets incorporados do bmp.txt
    for (size_t i = 0; i < g_embedded_assets_count; i++) {
        if (kstrcmp(clean_name, g_embedded_assets[i].name) == 0) {
            *out_size = (size_t)(g_embedded_assets[i].end - g_embedded_assets[i].start);
            return g_embedded_assets[i].start;
        }
    }

    return NULL;
}

int vfs_write_file(const char* filename, const uint8_t* content, size_t size) {
    const char* clean_name = filename;
    if (kstrncmp(filename, "#|ROOT*", 7) == 0) {
        clean_name = filename + 7;
    }
    return fat32_write_file(clean_name, content, size);
}
