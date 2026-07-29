FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    build-essential \
    xorriso \
    grub-pc-bin \
    grub-common

WORKDIR /os

# 1. Cria o disk.img de 64KB
# 2. Compila o boot.s (que ja carrega o disk.img via .incbin)
CMD dd if=/dev/zero of=disk.img bs=1k count=64 && \
    gcc -m32 -c boot.s -o boot.o -fno-pie -fno-pic && \
    gcc -m32 -c src/gfx.c -o gfx.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/memory.c -o memory.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/idt.c -o idt.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/serial.c -o serial.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/rtc.c -o rtc.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/vfs.c -o vfs.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -c src/kernel.c -o kernel.o -Iinclude -ffreestanding -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m32 -no-pie -fno-pic -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o gfx.o memory.o idt.o serial.o rtc.o vfs.o kernel.o -lgcc && \
    mkdir -p isodir/boot/grub && \
    cp myos.bin isodir/boot/myos.bin && \
    cp grub.cfg isodir/boot/grub/grub.cfg && \
    grub-mkrescue -o meu_so.iso isodir
