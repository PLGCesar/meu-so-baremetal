#!/bin/bash
echo "======================================================="
echo " CORRIGINDO A GDT DE 64-BITS (O TOQUE FINAL)           "
echo "======================================================="

# Substitui o descritor de Código quebrado pelo exato de x86_64
sed -i 's/.quad (1<<43) | (1<<44) | (1<<47) | (1<<53) # 64-bit Code/.quad 0x00209A0000000000/g' boot.s

# Substitui o descritor de Dados quebrado pelo exato (com bit Writable!)
sed -i 's/.quad (1<<44) | (1<<47)                     # 64-bit Data/.quad 0x0000920000000000/g' boot.s

echo "SUCESSO: GDT de 64-bits configurada perfeitamente!
