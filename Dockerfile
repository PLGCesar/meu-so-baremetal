FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    build-essential \
    xorriso \
    grub-pc-bin \
    grub-common

WORKDIR /os

# Cria a imagem de disco disk.img (para fotos) e harddisk.img de 1MB (HD IDE/ATA)
CMD dd if=/dev/zero of=disk.img bs=1k count=64 && \
    dd if=/dev/zero of=harddisk.img bs=1M count=1 && \
    gcc -m32 -c boot.s -o boot.o -fno-pie -fno-pic && \
    gcc -m32 -c src/gfx.c -o gfx.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/memory.c -o memory.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/idt.c -o idt.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/serial.c -o serial.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/rtc.c -o rtc.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/ata.c -o ata.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/vfs.c -o vfs.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/sound.c -o sound.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/music.c -o music.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/bmp.c -o bmp.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/ui.c -o ui.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/kernel.c -o kernel.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -no-pie -fno-pic -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o gfx.o memory.o idt.o serial.o rtc.o ata.o vfs.o sound.o music.o bmp.o ui.o kernel.o -lgcc && \
    mkdir -p isodir/boot/grub && \
    cp myos.bin isodir/boot/myos.bin && \
    cp grub.cfg isodir/boot/grub/grub.cfg && \
    grub-mkrescue -o meu_so.iso isodir
