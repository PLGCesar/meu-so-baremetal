#include "../include/ata.h"
#include "../include/ahci.h"
#include "../include/serial.h"
#include "../include/task.h"

static volatile int ata_lock = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret; asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void acquire_ata(void) {
    while (__sync_lock_test_and_set(&ata_lock, 1)) {
        if (get_num_tasks() > 1) task_yield();
        else asm volatile("pause");
    }
}
static void release_ata(void) {
    __sync_lock_release(&ata_lock);
}

static void ata_wait_bsy(void) {
    while (inb(0x1F7) & 0x80) {
        if (get_num_tasks() > 1) task_yield();
        else asm volatile("pause");
    }
}
static void ata_wait_drq(void) {
    while (!(inb(0x1F7) & 0x08)) {
        if (get_num_tasks() > 1) task_yield();
        else asm volatile("pause");
    }
}

void ata_init(void) {
    ahci_init();
    if (ahci_is_active()) {
        serial_write("[STORAGE] Camada de Disco Inicializada com Controlador AHCI SATA DMA!\n");
    } else {
        serial_write("[STORAGE] Camada de Disco Inicializada no modo ATA/IDE Legado!\n");
    }
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (ahci_is_active()) {
        return ahci_read_sector(lba, buffer);
    }

    acquire_ata();
    ata_wait_bsy();
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);

    ata_wait_bsy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(0x1F0);
        buffer[i * 2] = (uint8_t)(data & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)((data >> 8) & 0xFF);
    }
    release_ata();
    return 1;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (ahci_is_active()) {
        return ahci_write_sector(lba, buffer);
    }

    acquire_ata();
    ata_wait_bsy();
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);

    ata_wait_bsy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(0x01F0, data);
    }
    outb(0x1F7, 0xE7);
    ata_wait_bsy();
    release_ata();
    return 1;
}
