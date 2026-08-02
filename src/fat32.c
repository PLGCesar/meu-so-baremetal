#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../include/fat32.h"
#include "../include/ata.h"
#include "../include/serial.h"
#include "../include/util.h"
#include "../include/memory.h"

static fat32_bpb_t bpb;
static uint32_t fat_start_sector = 0;
static uint32_t cluster_start_sector = 0;
static uint8_t sector_buffer[512];

void fat32_init(void) {
    ata_read_sector(0, sector_buffer);
    kmemcpy(&bpb, sector_buffer, sizeof(fat32_bpb_t));
    if (bpb.bytes_per_sector != 512) {
        kmemset(sector_buffer, 0, 512);
        fat32_bpb_t* new_bpb = (fat32_bpb_t*)sector_buffer;
        new_bpb->bytes_per_sector = 512;
        new_bpb->sectors_per_cluster = 1;
        new_bpb->reserved_sectors = 32;
        new_bpb->fat_count = 2;
        new_bpb->sectors_per_fat_32 = 32;
        new_bpb->root_cluster = 2;
        kmemcpy(new_bpb->fs_type, "FAT32   ", 8);
        ata_write_sector(0, sector_buffer);
        kmemcpy(&bpb, new_bpb, sizeof(fat32_bpb_t));
    }
    fat_start_sector = bpb.reserved_sectors;
    cluster_start_sector = fat_start_sector + (bpb.fat_count * bpb.sectors_per_fat_32);
}

void fat32_list_files(char* out_buf, size_t max_len) {
    uint32_t root_lba = cluster_start_sector + ((bpb.root_cluster - 2) * bpb.sectors_per_cluster);
    ata_read_sector(root_lba, sector_buffer);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)sector_buffer;
    size_t pos = 0; out_buf[0] = '\0';
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] != 0x00 && (uint8_t)entries[i].name[0] != 0xE5) {
            for (int j = 0; j < 11; j++) {
                if (entries[i].name[j] != ' ' && pos < max_len - 3) out_buf[pos++] = entries[i].name[j];
            }
            out_buf[pos++] = ' '; out_buf[pos++] = ' ';
        }
    }
    out_buf[pos] = '\0';
}

const uint8_t* fat32_read_file(const char* filename, size_t* out_size) {
    (void)filename;
    (void)filename;
    uint32_t root_lba = cluster_start_sector + ((bpb.root_cluster - 2) * bpb.sectors_per_cluster);
    ata_read_sector(root_lba, sector_buffer);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)sector_buffer;
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] != 0x00 && (uint8_t)entries[i].name[0] != 0xE5) {
            uint32_t cluster = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
            uint32_t file_lba = cluster_start_sector + ((cluster - 2) * bpb.sectors_per_cluster);
            
            uint32_t size = entries[i].size;
            uint32_t sectors = (size + 511) / 512;
            if (sectors == 0) sectors = 1;
            
            uint8_t* file_buf = kmalloc(sectors * 512);
            if (!file_buf) return NULL;
            
            for(uint32_t s = 0; s < sectors; s++) ata_read_sector(file_lba + s, file_buf + (s * 512));
            *out_size = size;
            return file_buf;
        }
    }
    return NULL;
}

int fat32_write_file(const char* filename, const uint8_t* content, size_t size) {
    uint32_t root_lba = cluster_start_sector + ((bpb.root_cluster - 2) * bpb.sectors_per_cluster);
    ata_read_sector(root_lba, sector_buffer);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)sector_buffer;
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            kmemset(entries[i].name, ' ', 11);
            size_t fn_len = kstrlen(filename);
            if (fn_len > 11) fn_len = 11;
            kmemcpy(entries[i].name, filename, fn_len);

            uint32_t file_cluster = 3 + i;
            entries[i].cluster_low = file_cluster & 0xFFFF;
            entries[i].cluster_high = (file_cluster >> 16) & 0xFFFF;
            entries[i].size = size; entries[i].attr = 0x20;
            ata_write_sector(root_lba, sector_buffer);

            uint32_t file_lba = cluster_start_sector + ((file_cluster - 2) * bpb.sectors_per_cluster);
            uint32_t sectors = (size + 511) / 512;
            if (sectors == 0) sectors = 1;
            
            uint8_t* data_buf = kmalloc(sectors * 512);
            if (!data_buf) return 0;
            kmemset(data_buf, 0, sectors * 512);
            kmemcpy(data_buf, content, size);
            
            for(uint32_t s = 0; s < sectors; s++) ata_write_sector(file_lba + s, data_buf + (s * 512));
            kfree(data_buf);
            return 1;
        }
    }
    return 0;
}
