import os
import time
import subprocess

print("\n=======================================================")
print("   SERVIDOR WEB NO VNC DO KERNEL INICIALIZADO!         ")
print("=======================================================")
print("Porta Web 6080 aberta em 0.0.0.0!")
print("Acesse localmente em: http://localhost:6080/vnc.html")
print("=======================================================\n")

try:
    print("[TUNNEL] Gerando URL publica segura com Cloudflare...")
    proc = subprocess.Popen(
        ["cloudflared", "tunnel", "--url", "http://localhost:6080"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True
    )
    for line in proc.stdout:
        if "trycloudflare.com" in line:
            print("\n" + "="*65)
            print("  🔗 LINK PÚBLICO PARA ABRIR SEU SO NO NAVEGADOR:")
            for word in line.split():
                if "trycloudflare.com" in word:
                    print(f"  👉 {word}/vnc.html")
            print("="*65 + "\n")
            break
except Exception as e:
    print("[INFO] Acesse http://localhost:6080/vnc.html localmente.")

while True:
    time.sleep(3600)
