#!/bin/bash
echo "======================================================="
echo " LIMPANDO OS WARNINGS DO GCC (CLEAN CODE)              "
echo "======================================================="

# 1. Arruma as chaves {} e quebra de linha nos limites do mouse no idt.c
sed -i 's/if (mouse_x < 0) mouse_x = 0; if (mouse_x >= 800) mouse_x = 799;/if (mouse_x < 0) { mouse_x = 0; }\n            if (mouse_x >= 800) { mouse_x = 799; }/g' src/idt.c

sed -i 's/if (mouse_y < 0) mouse_y = 0; if (mouse_y >= 600) mouse_y = 599;/if (mouse_y < 0) { mouse_y = 0; }\n            if (mouse_y >= 600) { mouse_y = 599; }/g' src/idt.c

# 2. Remove a variável declarada e não utilizada no ui.c
sed -i 's/static int wallpaper = -1, start_open = 0, prev_mouse = 0;/static int start_open = 0, prev_mouse = 0;/g' src/ui.c

echo "SUCESSO: Warnings aniquilados!"
