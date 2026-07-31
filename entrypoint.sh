#!/bin/bash
# 1. Compila a ISO e cria os discos HD
dd if=/dev/zero of=disk.img bs=1k count=64
dd if=/dev/zero of=harddisk.img bs=1M count=1
gcc -m64 -c boot.s -o boot.o -fno-pie -fno-pic
gcc -m64 -c src/util.c -o util.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/gfx.c -o gfx.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/memory.c -o memory.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/idt.c -o idt.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/serial.c -o serial.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/rtc.c -o rtc.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/ata.c -o ata.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/fat32.c -o fat32.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/vfs.c -o vfs.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/sound.c -o sound.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/music.c -o music.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/bmp.c -o bmp.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/task.c -o task.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/elf.c -o elf.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/net.c -o net.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/ui.c -o ui.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -c src/kernel.c -o kernel.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra
gcc -m64 -no-pie -fno-pic -T linker.ld -o myos.bin -ffreestanding -mno-red-zone -O2 -nostdlib boot.o util.o gfx.o memory.o idt.o serial.o rtc.o ata.o fat32.o vfs.o sound.o music.o bmp.o task.o elf.o net.o ui.o kernel.o -lgcc
mkdir -p isodir/boot/grub
cp myos.bin isodir/boot/myos.bin
cp grub.cfg isodir/boot/grub/grub.cfg
grub-mkrescue -o meu_so.iso isodir

# 2. Inicia o QEMU
qemu-system-x86_64 -cdrom meu_so.iso -drive file=harddisk.img,format=raw,if=ide,index=0,media=disk -vga std -vnc 127.0.0.1:0 -serial stdio -netdev user,id=net0 -device rtl8139,netdev=net0 &

# 3. Conecta o noVNC na porta 6080 para 0.0.0.0
websockify --web /usr/share/novnc 6080 127.0.0.1:5900 &

# 4. Executa o Script Python que descobre a URL pública
python3 server.py
