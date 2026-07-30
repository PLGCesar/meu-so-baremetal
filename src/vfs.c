#include "../include/vfs.h"
#include "../include/fat32.h"
#include "../include/serial.h"

extern uint8_t foto_bmp_start[];

void vfs_init(void) {
    fat32_init();
    serial_write("[VFS] VFS Conectado ao Driver de Disco FAT32!\n");
}

void vfs_list(char* out_buf, size_t max_len) {
    fat32_list_files(out_buf, max_len);
}

const uint8_t* vfs_read(const char* filename, size_t* out_size) {
    if (filename[0] == 'f' && filename[1] == 'o' && filename[2] == 't') {
        *out_size = 30000;
        return foto_bmp_start;
    }
    return fat32_read_file(filename, out_size);
}

int vfs_write_file(const char* filename, const uint8_t* content, size_t size) {
    return fat32_write_file(filename, content, size);
}
