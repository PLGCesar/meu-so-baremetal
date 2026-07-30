#include "../include/net.h"
#include "../include/serial.h"
#include "../include/util.h"
#include "../include/memory.h"

static uint16_t io_base = 0;
static uint8_t mac_addr[6];
static uint32_t my_ip = 0x0F02000A; // IP OFICIAL DO QEMU SLIRP NAT: 10.0.2.15
static uint8_t* rx_buffer = NULL;
static uint32_t rx_offset = 0;
static uint32_t rx_count = 0;
static uint32_t tx_count = 0;

static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint16_t inw(uint16_t port) { uint16_t ret; asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
static inline void outw(uint16_t port, uint16_t val) { asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint32_t inl(uint16_t port) { uint32_t ret; asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
static inline void outl(uint16_t port, uint32_t val) { asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port)); }

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0x0CF8, address);
    return inl(0x0CFC);
}

void net_send_packet(const uint8_t* packet, uint32_t len) {
    if (!io_base) return;

    static uint8_t tx_buffer_num = 0;
    uint16_t tx_reg = 0x20 + (tx_buffer_num * 4);
    uint16_t tx_status_reg = 0x10 + (tx_buffer_num * 4);

    outl(io_base + tx_reg, (uint32_t)(uintptr_t)packet);
    outl(io_base + tx_status_reg, len & 0xFFF);

    tx_buffer_num = (tx_buffer_num + 1) % 4;
    tx_count++;
    serial_write("[NET] Pacote Ethernet transmitido via RTL8139!\n");
}

uint16_t ip_checksum(void* vdata, size_t length) {
    char* data = (char*)vdata;
    uint32_t acc = 0xffff;
    for (size_t i = 0; i + 1 < length; i += 2) {
        uint16_t word;
        kmemcpy(&word, data + i, 2);
        acc += __builtin_bswap16(word);
        if (acc > 0xffff) acc -= 0xffff;
    }
    if (length & 1) {
        uint16_t word = 0;
        kmemcpy(&word, data + length - 1, 1);
        acc += __builtin_bswap16(word);
        if (acc > 0xffff) acc -= 0xffff;
    }
    return __builtin_bswap16(~acc);
}

void net_init(void) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read_config(0, slot, 0, 0);
        if ((id & 0xFFFF) == 0x10EC && ((id >> 16) & 0xFFFF) == 0x8139) {
            uint32_t bar0 = pci_read_config(0, slot, 0, 0x10);
            io_base = (uint16_t)(bar0 & ~0x3);
            serial_write("[NET] Placa de Rede Realtek RTL8139 Encontrada no Barramento PCI!\n");
            break;
        }
    }

    if (!io_base) {
        io_base = 0xC000;
    }

    outb(io_base + 0x52, 0x00);
    outb(io_base + 0x37, 0x10);
    while ((inb(io_base + 0x37) & 0x10) != 0);

    for (int i = 0; i < 6; i++) {
        mac_addr[i] = inb(io_base + i);
    }

    rx_buffer = kmalloc(8192 + 16 + 1500);
    outl(io_base + 0x30, (uint32_t)(uintptr_t)rx_buffer);

    outw(io_base + 0x3C, 0x0005);
    outl(io_base + 0x44, 0x0F);
    outb(io_base + 0x37, 0x0C);

    serial_write("[NET] Placa RTL8139 Inicializada com IP 10.0.2.15!\n");
}

void net_poll(void) {
    if (!io_base || !rx_buffer) return;

    if ((inb(io_base + 0x37) & 0x01) == 0) {
        uint8_t* pkt = rx_buffer + rx_offset;
        ethernet_header_t* eth = (ethernet_header_t*)(pkt + 4);

        rx_count++;
        uint16_t eth_type = __builtin_bswap16(eth->type);

        if (eth_type == 0x0806) { // ARP
            arp_packet_t* arp = (arp_packet_t*)(pkt + 4 + sizeof(ethernet_header_t));
            if (__builtin_bswap16(arp->opcode) == 1 && arp->dest_ip == my_ip) {
                uint8_t reply[64];
                ethernet_header_t* r_eth = (ethernet_header_t*)reply;
                arp_packet_t* r_arp = (arp_packet_t*)(reply + sizeof(ethernet_header_t));

                kmemcpy(r_eth->dest_mac, eth->src_mac, 6);
                kmemcpy(r_eth->src_mac, mac_addr, 6);
                r_eth->type = __builtin_bswap16(0x0806);

                r_arp->hw_type = __builtin_bswap16(1);
                r_arp->proto_type = __builtin_bswap16(0x0800);
                r_arp->hw_len = 6; r_arp->proto_len = 4;
                r_arp->opcode = __builtin_bswap16(2);
                kmemcpy(r_arp->src_mac, mac_addr, 6);
                r_arp->src_ip = my_ip;
                kmemcpy(r_arp->dest_mac, arp->src_mac, 6);
                r_arp->dest_ip = arp->src_ip;

                net_send_packet(reply, sizeof(ethernet_header_t) + sizeof(arp_packet_t));
                serial_write("[NET] Resposta ARP Reply Enviada!\n");
            }
        } else if (eth_type == 0x0800) { // IPv4 / ICMP PING
            ipv4_header_t* ip = (ipv4_header_t*)(pkt + 4 + sizeof(ethernet_header_t));
            if (ip->protocol == 1 && ip->dest_ip == my_ip) {
                icmp_header_t* icmp = (icmp_header_t*)(pkt + 4 + sizeof(ethernet_header_t) + sizeof(ipv4_header_t));
                if (icmp->type == 8) {
                    serial_write("[NET] PING RECEBIDO! ENVIANDO ECHO REPLY...\n");

                    uint8_t reply[128];
                    kmemcpy(reply, pkt + 4, 98);

                    ethernet_header_t* r_eth = (ethernet_header_t*)reply;
                    ipv4_header_t* r_ip = (ipv4_header_t*)(reply + sizeof(ethernet_header_t));
                    icmp_header_t* r_icmp = (icmp_header_t*)(reply + sizeof(ethernet_header_t) + sizeof(ipv4_header_t));

                    kmemcpy(r_eth->dest_mac, eth->src_mac, 6);
                    kmemcpy(r_eth->src_mac, mac_addr, 6);

                    r_ip->dest_ip = ip->src_ip;
                    r_ip->src_ip = my_ip;
                    r_ip->checksum = 0;
                    r_ip->checksum = ip_checksum(r_ip, sizeof(ipv4_header_t));

                    r_icmp->type = 0;
                    r_icmp->checksum = 0;
                    r_icmp->checksum = ip_checksum(r_icmp, 64);

                    net_send_packet(reply, 98);
                    serial_write("[NET] PING ECHO REPLY ENVIADO COM SUCESSO!\n");
                }
            }
        }

        rx_offset = (rx_offset + 1536) % 8192;
        outw(io_base + 0x38, rx_offset - 16);
    }
}

const uint8_t* net_get_mac(void) { return mac_addr; }
uint32_t net_get_ip(void) { return my_ip; }
uint32_t net_get_rx_count(void) { return rx_count; }
uint32_t net_get_tx_count(void) { return tx_count; }
