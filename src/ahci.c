#include "../include/ahci.h"
#include "../include/serial.h"
#include "../include/memory.h"
#include "../include/klibc.h"

static hba_mem_t* abar = NULL;
static int active_sata_port = -1;
static int ahci_initialized = 0;

static inline void outl(uint16_t port, uint32_t val) { asm volatile ("outl %k0, %1" : : "a"(val), "Nd"(port)); }
static inline uint32_t inl(uint16_t port) { uint32_t ret; asm volatile ("inl %1, %k0" : "=a"(ret) : "Nd"(port)); return ret; }

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0x0CF8, address);
    return inl(0x0CFC);
}

static void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0x0CF8, address);
    outl(0x0CFC, val);
}

static int check_port_type(hba_port_t* port) {
    if (!port) return AHCI_DEV_NULL;
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE)
        return AHCI_DEV_NULL;

    return port->sig;
}

static int port_rebase(hba_port_t* port, int portno) {
    (void)portno;
    port->cmd &= ~0x0001; // CMD_ST
    port->cmd &= ~0x0010; // CMD_FRE

    // Timeout de segurança para evitar loop infinito/tela preta
    int timeout = 100000;
    while ((port->cmd & 0x4000) && --timeout > 0) {
        asm volatile("pause");
    }
    if (timeout <= 0) {
        serial_write("[AHCI] Timeout de porta em port_rebase. Abortando AHCI.\n");
        return 0;
    }

    void* raw_clb = kmalloc(4096 + 1024);
    uintptr_t clb_aligned = ((uintptr_t)raw_clb + 1023) & ~1023;
    fast_memset((void*)clb_aligned, 0, 1024);

    port->clb = (uint32_t)clb_aligned;
    port->clbu = 0;

    void* raw_fb = kmalloc(4096 + 256);
    uintptr_t fb_aligned = ((uintptr_t)raw_fb + 255) & ~255;
    fast_memset((void*)fb_aligned, 0, 256);

    port->fb = (uint32_t)fb_aligned;
    port->fbu = 0;

    hba_cmd_header_t* cmdheader = (hba_cmd_header_t*)clb_aligned;
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 1;
        void* raw_tbl = kmalloc(4096 + 128);
        uintptr_t tbl_aligned = ((uintptr_t)raw_tbl + 127) & ~127;
        fast_memset((void*)tbl_aligned, 0, sizeof(hba_cmd_tbl_t));
        cmdheader[i].ctba = (uint32_t)tbl_aligned;
        cmdheader[i].ctbau = 0;
    }

    port->cmd |= 0x0010; // CMD_FRE
    port->cmd |= 0x0001; // CMD_ST
    return 1;
}

void ahci_init(void) {
    serial_write("[AHCI] Procurando Controlador AHCI SATA PCI...\n");
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read_config(bus, slot, func, 0x00);
                if (id == 0xFFFFFFFF || id == 0) continue;

                uint32_t class_reg = pci_read_config(bus, slot, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass   = (class_reg >> 16) & 0xFF;

                if (class_code == 0x01 && subclass == 0x06) {
                    serial_write("[AHCI] Controlador AHCI SATA PCI Encontrado!\n");

                    // Habilita IO, Memory Space e Bus Master DMA
                    uint32_t pci_cmd = pci_read_config(bus, slot, func, 0x04);
                    pci_cmd |= 0x07;
                    pci_write_config(bus, slot, func, 0x04, pci_cmd);

                    uint32_t bar5 = pci_read_config(bus, slot, func, 0x24);
                    uintptr_t bar_addr = bar5 & 0xFFFFFFF0;

                    // CORREÇÃO CRÍTICA: Se o BAR5 for 0, atribui 0xFE000000 no registrador do dispositivo PCI
                    if (bar_addr == 0 || bar_addr == 0xFFFFFFF0) {
                        bar_addr = 0xFE000000;
                        pci_write_config(bus, slot, func, 0x24, (uint32_t)bar_addr);
                    }

                    abar = (hba_mem_t*)bar_addr;
                    abar->ghc |= (1 << 31); // Global AHCI Enable

                    uint32_t pi = abar->pi;
                    for (int i = 0; i < 32; i++) {
                        if (pi & (1 << i)) {
                            int dt = check_port_type(&abar->ports[i]);
                            if (dt == AHCI_DEV_SATA) {
                                serial_write("[AHCI] Disco SATA Ativo Detectado no Port!\n");
                                if (port_rebase(&abar->ports[i], i)) {
                                    active_sata_port = i;
                                    ahci_initialized = 1;
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    serial_write("[AHCI] Nenhum controlador SATA AHCI pronto. Ativando Fallback IDE.\n");
}

int ahci_is_active(void) {
    return ahci_initialized && active_sata_port >= 0;
}

const char* ahci_get_status_string(void) {
    if (ahci_is_active()) return "SATA AHCI DMA (6 Gbps) - ATIVO";
    return "ATA IDE LEGADO (PIO Mode) - FALLBACK";
}

int ahci_read_sector(uint32_t lba, uint8_t* buffer) {
    if (!ahci_is_active() || !buffer) return 0;

    hba_port_t* port = &abar->ports[active_sata_port];
    port->is = (uint32_t)-1;

    hba_cmd_header_t* cmdheader = (hba_cmd_header_t*)(uintptr_t)port->clb;
    cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t);
    cmdheader->w = 0;
    cmdheader->prdtl = 1;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)(uintptr_t)cmdheader->ctba;
    fast_memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));

    cmdtbl->prdt_entry[0].dba = (uint32_t)(uintptr_t)buffer;
    cmdtbl->prdt_entry[0].dbau = 0;
    cmdtbl->prdt_entry[0].dbc = 511;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
    cmdfis->fis_type = 0x27;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_READ_DMA_EX;

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = 0;
    cmdfis->lba5 = 0;

    cmdfis->countl = 1;
    cmdfis->counth = 0;

    port->ci = 1;

    int timeout = 1000000;
    while (--timeout > 0) {
        if ((port->ci & 1) == 0) break;
        if (port->is & (1 << 30)) return 0;
    }
    if (timeout <= 0) return 0;

    return 1;
}

int ahci_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!ahci_is_active() || !buffer) return 0;

    hba_port_t* port = &abar->ports[active_sata_port];
    port->is = (uint32_t)-1;

    hba_cmd_header_t* cmdheader = (hba_cmd_header_t*)(uintptr_t)port->clb;
    cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t);
    cmdheader->w = 1;
    cmdheader->prdtl = 1;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)(uintptr_t)cmdheader->ctba;
    fast_memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));

    cmdtbl->prdt_entry[0].dba = (uint32_t)(uintptr_t)buffer;
    cmdtbl->prdt_entry[0].dbau = 0;
    cmdtbl->prdt_entry[0].dbc = 511;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
    cmdfis->fis_type = 0x27;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_WRITE_DMA_EX;

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = 0;
    cmdfis->lba5 = 0;

    cmdfis->countl = 1;
    cmdfis->counth = 0;

    port->ci = 1;

    int timeout = 1000000;
    while (--timeout > 0) {
        if ((port->ci & 1) == 0) break;
        if (port->is & (1 << 30)) return 0;
    }
    if (timeout <= 0) return 0;

    return 1;
}
