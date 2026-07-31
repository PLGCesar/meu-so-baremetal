FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    build-essential \
    xorriso \
    grub-pc-bin \
    grub-common

WORKDIR /os

CMD dd if=/dev/zero of=disk.img bs=1k count=64 && \
    dd if=/dev/zero of=harddisk.img bs=1M count=1 && \
    gcc -m64 -c boot.s -o boot.o -fno-pie -fno-pic && \
    gcc -m64 -c src/util.c -o util.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/gfx.c -o gfx.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/memory.c -o memory.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/idt.c -o idt.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/serial.c -o serial.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/rtc.c -o rtc.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/ata.c -o ata.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/fat32.c -o fat32.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/vfs.c -o vfs.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/sound.c -o sound.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/music.c -o music.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/bmp.c -o bmp.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/task.c -o task.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/elf.c -o elf.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/net.c -o net.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/syscall.c -o syscall.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/ui.c -o ui.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -c src/kernel.c -o kernel.o -Iinclude -ffreestanding -mno-red-zone -fno-pie -fno-pic -O2 -Wall -Wextra && \
    gcc -m64 -no-pie -fno-pic -T linker.ld -o myos.bin -ffreestanding -mno-red-zone -O2 -nostdlib boot.o util.o gfx.o memory.o idt.o serial.o rtc.o ata.o fat32.o vfs.o sound.o music.o bmp.o task.o elf.o net.o syscall.o ui.o kernel.o -lgcc && \
    mkdir -p isodir/boot/grub && \
    cp myos.bin isodir/boot/myos.bin && \
    cp grub.cfg isodir/boot/grub/grub.cfg && \
    grub-mkrescue -o meu_so.iso isodir
