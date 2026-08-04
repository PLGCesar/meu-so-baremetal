<div align="center">

# 🦫 CapivaraOS

**Sistema Operacional Bare-Metal de 64-Bits escrito do zero em C e Assembly**

[![Build ISO](https://img.shields.io/badge/Build-GitHub%20Actions-blue?logo=githubactions&logoColor=white)](#-compilação-e-build-automático)
[![Architecture](https://img.shields.io/badge/Architecture-x86__64%20%28Long%20Mode%29-orange?logo=cpu)](#-arquitetura--engenharia)
[![UI](https://img.shields.io/badge/GUI-Window%20Manager%201024x768-success)](#-interface-gráfica--aplicativos)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

*O sistema operacional mais inquebrável, calmo e otimizado da comunidade bare-metal.*

</div>

---

## 📌 Sobre o Projeto

O **CapivaraOS** é um projeto de estudo e engenharia de baixo nível focado no desenvolvimento de um **Kernel monolítico de 64-bits (Long Mode)** do zero, sem depender de bibliotecas externas (`nostdlib`, `freestanding`). 

Diferente de sistemas conceituais em modo texto, o CapivaraOS inicializa diretamente em alta resolução VBE (1024x768x32bpp) com um **Gerenciador de Janelas flutuantes estilo Desktop**, sistema de arquivos FAT32, placa de rede Realtek RTL8139, player de vídeo nativo e suporte a instruções modernas de Syscall/Sysret da arquitetura AMD64/x86_64.

---

## ✨ Recursos do Sistema (Features)

### 🖥️ Interface Gráfica (Desktop & GUI)
- **Window Manager Nativo:** Janelas flutuantes com sombras, arrastar e soltar (Drag & Drop), profundidade de camada (*Z-Indexing*) e botão de fechar.
- **Double Buffer Otimizado:** Troca de buffers no barramento gráfico feita por instruções nativas em Assembly (`rep movsq`).
- **Renderização de Cursor & Wallpapers:** Cursor do mouse com canal Alpha (transparência), suporte a fotos `.bmp`, paisagens matematicamente geradas e animações `.bgif`.

### 🎮 Aplicativos Integrados (Apps Nativos)
1. 💻 **Terminal Shell VFS:** Leitura/escrita de arquivos com sintaxe personalizada (`#|ROOT*`).
2. 🧠 **RAM Monitor:** Diagnóstico em tempo real da alocação de Heap.
3. 🌐 **Diagnóstico de Rede:** Monitor de tráfego de pacotes ethernet e status da placa PCI.
4. 🖼️ **Galeria de Imagens:** Leitor e renderizador nativo de arquivos bitmap `.bmp` de 24-bits.
5. 🎵 **Chiptune Player:** Sintetizador de áudio 8-bit com temas de Tetris e Super Mario via PC Speaker.
6. ⚙️ **Gerenciador de Tarefas:** Monitoramento de processos em multitasking Round-Robin.
7. 🎨 **Paint Studio:** Desenhe na tela e exporte sua arte diretamente para arquivos `.bmp` salvos no HD.
8. 📁 **Explorador VFS:** Navegação e inspeção visual do diretório de arquivos.
9. 🎬 **Player de Vídeo BGIF:** Reprodutor de animações customizadas (`.bgif`) a 24 FPS.
10. 🧮 **Calculadora Baremetal:** Calculadora matemática totalmente integrada à UI e operada via mouse.

---

## ⚙️ Arquitetura & Engenharia de Baixo Nível

- **NÚCLEO 64-BITS (Long Mode):** Bootloader Multiboot1, inicialização de tabela de páginas (PML4, PDP, PD) mapeando memória física em blocos de 2MB.
- **`klibc` PRÓPRIA:** Implementação customizada de `memcpy`, `memset` e manipuladores de string utilizando **Inline Assembly x86_64** (`rep movsq`/`rep stosq`) para transferência de blocos de 8 bytes por ciclo.
- **SYSCALLS DE ALTA VELOCIDADE:** Configuração dos MSRs `EFER`, `STAR`, `LSTAR` e `SFMASK` permitindo comunicação segura Ring 0 / Ring 3 via instruções `syscall` e `sysretq`.
- **GERENCIAMENTO DE MEMÓRIA:** Heap Allocator duplamente encadeado com alinhamento rigoroso de 16-bytes exigido pela ABI x86_64 e fusão de memória *O(1)* no `kfree`.
- **DRIVERS DE HARDWARE:**
  - **Rede:** Driver Realtek RTL8139 PCI com escuta de rede e resposta automática a pacotes **ARP Reply** e **ICMP Echo (Ping)**.
  - **Disco:** Driver ATA/IDE de bloco lendo e gravando setores do HD físico/virtual.
  - **FAT32:** Leitura e gravação do sistema de arquivos FAT32 montado em LBA.
  - **RTC:** Leitura do chip CMOS com conversão BCD -> Binário e ajuste do Horário de Brasília (BRT / UTC-3).
  - **Entrada:** Interrupções PIC remapeadas para Teclado PS/2 e Mouse PS/2 com tratamento de pacotes de 3-bytes e limites de tela.

---

## 🚀 Como Executar o CapivaraOS

### 1. 📥 Baixando a ISO Pronta
O projeto utiliza **GitHub Actions** para compilar a ISO automaticamente a cada atualização. Basta ir na aba **Actions** ou **Releases** deste repositório e baixar o arquivo `meu_so.iso`.

### 2. ⚡ Executando via QEMU
Com o [QEMU](https://www.qemu.org/) instalado, execute o comando oficial no seu terminal:

```bash
qemu-system-x86_64 \
  -cdrom meu_so.iso \
  -drive file=harddisk.img,format=raw,if=ide,index=0,media=disk \
  -vga std \
  -vnc :0 \
  -serial stdio \
  -netdev user,id=net0 \
  -device rtl8139,netdev=net0 \
  -audiodev pa,id=audio0 \
  -machine pcspk-audiodev=audio0```


# ESTRUTURA DE ARQUIVOS
├── boot.s               # Ponto de entrada Assembly 32->64b, GDT, Paging e IDT Stubs
├── Dockerfile           # Ambiente isolado de build com GCC x86_64 & GRUB
├── grub.cfg             # Configuração do Bootloader Multiboot (GRUB)
├── linker.ld            # Linker Script para mapeamento do Kernel em 1MB
├── include/             # Cabeçalhos do Kernel (.h)
│   ├── klibc.h          # Biblioteca padrão C customizada do Kernel
│   ├── gfx.h            # Motor Gráfico e primitivas
│   ├── memory.h         # Gerenciador de Heap Alinhado 16b
│   ├── net.h            # Pilha de Rede RTL8139, ARP e ICMP
│   └── ...
├── src/                 # Código-fonte em C (.c)
│   ├── klibc.c          # Otimizações em Inline Assembly (rep movsq)
│   ├── ui.c             # Window Manager, Apps e Desktops
│   ├── kernel.c         # Ponto de Entrada do Kernel
│   └── ...
└── tools/               # Utilitários (Gerador de Vídeos .bgif)	
