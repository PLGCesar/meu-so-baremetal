/* Multiboot 1 com solicitação de Modo Gráfico */
.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set GRAPHICS, 1<<2
.set FLAGS,    ALIGN | MEMINFO | GRAPHICS
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
/* Solicitação de Modo Gráfico 800x600x32 bpp ao Bootloader */
.long 0   /* 0 = Linear Graphics Mode */
.long 800 /* Largura */
.long 600 /* Altura */
.long 32  /* Profundidade de Cores (32-bit RGB) */

.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KB de Pilha
stack_top:

.section .text
.global _start
.type _start, @function
_start:
	mov $stack_top, %esp
	push %ebx            # Passa o ponteiro da estrutura Multiboot para o C!
	call kernel_main
	cli
1:	hlt
	jmp 1b
