#!/bin/bash
echo "=========================================================="
echo "    INICIALIZANDO SERVIDOR WEB NOVNC NO GITHUB ACTIONS    "
echo "=========================================================="

# 1. Compila a ISO e prepara discos
dd if=/dev/zero of=disk.img bs=1k count=64 > /dev/null 2>&1
dd if=/dev/zero of=harddisk.img bs=1M count=1 > /dev/null 2>&1
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
grub-mkrescue -o meu_so.iso isodir > /dev/null 2>&1

# 2. Inicia QEMU e noVNC
qemu-system-x86_64 -cdrom meu_so.iso -drive file=harddisk.img,format=raw,if=ide,index=0,media=disk -vga std -vnc 127.0.0.1:0 -serial stdio -netdev user,id=net0 -device rtl8139,netdev=net0 > /dev/null 2>&1 &
websockify --web /usr/share/novnc 6080 127.0.0.1:5900 > /dev/null 2>&1 &

sleep 2

# 3. Baixa Cloudflared se necessario
if [ ! -f /usr/local/bin/cloudflared ]; then
    curl -sL https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -o /usr/local/bin/cloudflared
    chmod +x /usr/local/bin/cloudflared
fi

# 4. Inicia o Cloudflared escrevendo o log em segundo plano
cloudflared tunnel --url http://localhost:6080 > cloudflare.log 2>&1 &

echo "Aguardando 10 segundos para o registro e propagacao mundial de DNS..."
sleep 10

# Extrai o link gerado
URL=$(grep -o 'https://[-a-zA-Z0-9.]*\.trycloudflare\.com' cloudflare.log | head -n 1)

echo ""
echo "================================================================="
echo "  🔗 SEU LINK PÚBLICO PARA ABRIR O SO NO NAVEGADOR DO CELULAR:  "
echo "================================================================="
echo "👉 ${URL}/vnc.html"
echo "================================================================="
echo "(Se ao clicar der erro de DNS, aguarde 15 segundos e recarregue a pagina)"
echo ""

# MANTÉM O CONTAINER E O TÚNEL VIVOS NO GITHUB ACTIONS
while true; do
    sleep 3600
done
