#include "../include/ata.h"
#include "../include/serial.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void ata_wait_bsy(void) {
    while (inb(0x1F7) & 0x80); // Aguarda Bit 7 (BSY) desativar
}

static void ata_wait_drq(void) {
    while (!(inb(0x1F7) & 0x08)); // Aguarda Bit 3 (DRQ) ativar
}

void ata_init(void) {
    serial_write("[ATA DISK] Driver de Disco Rigido IDE/ATA Inicializado!\n");
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // Drive Master + LBA bits 24-27
    outb(0x1F2, 1);                           // Lê 1 setor (512 bytes)
    outb(0x1F3, (uint8_t)lba);               // LBA bits 0-7
    outb(0x1F4, (uint8_t)(lba >> 8));        // LBA bits 8-15
    outb(0x1F5, (uint8_t)(lba >> 16));       // LBA bits 16-23
    outb(0x1F7, 0x20);                        // Comando 0x20: READ SECTORS

    ata_wait_bsy();
    ata_wait_drq();

    // Lê 256 palavras de 16 bits (512 bytes) da porta de dados 0x1F0
    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(0x1F0);
        buffer[i * 2] = (uint8_t)(data & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)((data >> 8) & 0xFF);
    }

    return 1;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    ata_wait_bsy();

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);                           // Escreve 1 setor (512 bytes)
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);                        // Comando 0x30: WRITE SECTORS

    ata_wait_bsy();
    ata_wait_drq();

    // Envia os 512 bytes para a porta 0x1F0
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(0x01F0, data);
    }

    // Comando Cache Flush (0xE7) para gravação física permanente no HD
    outb(0x1F7, 0xE7);
    ata_wait_bsy();

    serial_write("[ATA DISK] Setor gravado com sucesso no HD de hardware!\n");
    return 1;
}
