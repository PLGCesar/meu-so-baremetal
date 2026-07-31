FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    build-essential \
    xorriso \
    grub-pc-bin \
    grub-common \
    qemu-system-x86 \
    novnc \
    websockify \
    python3 \
    curl \
    wget

# Instala o Cloudflared para criar o Link Público automático
RUN curl -L --output cloudflared.deb https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb && \
    dpkg -i cloudflared.deb && \
    rm cloudflared.deb

WORKDIR /os

EXPOSE 6080 5900

CMD ["/bin/bash", "entrypoint.sh"]
