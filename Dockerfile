FROM ubuntu:22.04

# Instala compiladores, xorriso e GRUB para gerar a ISO
RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    build-essential \
    bison \
    flex \
    libgmp3-dev \
    libmpc-dev \
    libmpfr-dev \
    texinfo \
    xorriso \
    grub-pc-bin \
    grub-common

WORKDIR /os

# Comando que será executado para compilar a ISO
CMD gcc -m32 -c boot.s -o boot.o && \
    gcc -m32 -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra && \
    gcc -m32 -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o kernel.o -lgcc && \
    mkdir -p isodir/boot/grub && \
    cp myos.bin isodir/boot/myos.bin && \
    cp grub.cfg isodir/boot/grub/grub.cfg && \
    grub-mkrescue -o meu_so.iso isodir
