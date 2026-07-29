FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    build-essential \
    xorriso \
    grub-pc-bin \
    grub-common

WORKDIR /os

CMD gcc -m32 -c boot.s -o boot.o && \
    gcc -m32 -c src/gfx.c -o gfx.o -Iinclude -ffreestanding -O2 -Wall -Wextra && \
    gcc -m32 -c src/memory.c -o memory.o -Iinclude -ffreestanding -O2 -Wall -Wextra && \
    gcc -m32 -c src/idt.c -o idt.o -Iinclude -ffreestanding -O2 -Wall -Wextra && \
    gcc -m32 -c src/serial.c -o serial.o -Iinclude -ffreestanding -O2 -Wall -Wextra && \
    gcc -m32 -c src/kernel.c -o kernel.o -Iinclude -ffreestanding -O2 -Wall -Wextra && \
    gcc -m32 -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o gfx.o memory.o idt.o serial.o kernel.o -lgcc && \
    mkdir -p isodir/boot/grub && \
    cp myos.bin isodir/boot/myos.bin && \
    cp grub.cfg isodir/boot/grub/grub.cfg && \
    grub-mkrescue -o meu_so.iso isodir
