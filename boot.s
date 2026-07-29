/* Padrão Multiboot 1 */
.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

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
	call kernel_main
	cli
1:	hlt
	jmp 1b

/* --- SUPORTE A INTERRUPÇÕES DA CPU E TECLADO --- */
.global load_idt
load_idt:
    mov 4(%esp), %eax
    lidt (%eax)          # Carrega a tabela IDT na CPU
    sti                  # Ativa as interrupções na CPU
    ret

.global irq1_stub
.extern keyboard_handler_main
irq1_stub:
    pusha                # Salva os registradores gerais (EAX, ECX, EDX, EBX, etc.)
    cld                  # Limpa o Direction Flag (Obrigatório para código C no x86)

    # Salva os registradores de segmento
    push %ds
    push %es
    push %fs
    push %gs

    # Garante que os segmentos apontam para o Segmento de Dados do Kernel (0x10)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call keyboard_handler_main

    # Restaura os registradores de segmento
    pop %gs
    pop %fs
    pop %es
    pop %ds

    popa                 # Restaura os registradores gerais
    iret                 # Retorna da interrupção em segurança
