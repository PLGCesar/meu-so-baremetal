#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../include/vfs.h"
#include "../include/fat32.h"
#include "../include/serial.h"
#include "../include/util.h"

extern uint8_t foto_bmp_start[];
extern uint8_t bgif_anim_start[];
extern uint8_t bgif_anim_end[];
extern uint8_t app_elf_start[];
extern uint8_t app_elf_end[];

void vfs_init(void) {
    fat32_init();
    serial_write("[VFS] VFS Conectado com Suporte a Executaveis Ring 3!\n");
}

void vfs_list(char* out_buf, size_t max_len) {
    (void)max_len;
    char fat_buf[128];
    fat32_list_files(fat_buf, 128);

    // Adiciona os arquivos do VFS + o app.elf
    kstrcpy(out_buf, "app.elf  animacao.bgif  foto.bmp  ");
}

void vfs_list_custom_format(char* out_buf, size_t max_len) {
    (void)max_len;
    out_buf[0] = '\0';
    kstrcpy(out_buf, "#|ROOT*app.elf\n#|ROOT*animacao.bgif\n#|ROOT*foto.bmp\n");
}

const uint8_t* vfs_read(const char* filename, size_t* out_size) {
    const char* clean_name = filename;
    if (kstrncmp(filename, "#|ROOT*", 7) == 0) {
        clean_name = filename + 7;
    }

    if (kstrcmp(clean_name, "app.elf") == 0) {
        *out_size = (size_t)(app_elf_end - app_elf_start);
        return app_elf_start;
    }

    if (kstrcmp(clean_name, "animacao.bgif") == 0) {
        *out_size = (size_t)(bgif_anim_end - bgif_anim_start);
        return bgif_anim_start;
    }

    if (clean_name[0] == 'f' && clean_name[1] == 'o' && clean_name[2] == 't') {
        *out_size = 30000;
        return foto_bmp_start;
    }
    return fat32_read_file(clean_name, out_size);
}

int vfs_write_file(const char* filename, const uint8_t* content, size_t size) {
    const char* clean_name = filename;
    if (kstrncmp(filename, "#|ROOT*", 7) == 0) {
        clean_name = filename + 7;
    }
    return fat32_write_file(clean_name, content, size);
}
