#include "../include/vfs.h"
#include "../include/fat32.h"
#include "../include/serial.h"
#include "../include/util.h"

extern uint8_t foto_bmp_start[];
extern uint8_t bgif_anim_start[];
extern uint8_t bgif_anim_end[];

void vfs_init(void) {
    fat32_init();
    serial_write("[VFS] VFS Conectado com Suporte a .BMP-GIF!\n");
}

void vfs_list(char* out_buf, size_t max_len) {
    fat32_list_files(out_buf, max_len);
}

void vfs_list_custom_format(char* out_buf, size_t max_len) {
    char raw_list[128];
    fat32_list_files(raw_list, 128);

    out_buf[0] = '\0';
    size_t pos = 0;

    const char* prefix = "#|ROOT*";
    for (int k = 0; prefix[k] != '\0'; k++) out_buf[pos++] = prefix[k];

    for (size_t i = 0; raw_list[i] != '\0' && pos < max_len - 10; i++) {
        if (raw_list[i] == ' ' && raw_list[i+1] == ' ') {
            out_buf[pos++] = ' ';
            out_buf[pos++] = '\n';
            for (int k = 0; prefix[k] != '\0'; k++) out_buf[pos++] = prefix[k];
            i++;
        } else {
            out_buf[pos++] = raw_list[i];
        }
    }
    out_buf[pos] = '\0';
}

const uint8_t* vfs_read(const char* filename, size_t* out_size) {
    const char* clean_name = filename;
    if (kstrncmp(filename, "#|ROOT*", 7) == 0) {
        clean_name = filename + 7;
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
