.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 4096
pml4_table: .skip 4096
pdp_table: .skip 4096
pd_table: .skip 16384    # 4 Tabelas de 4KB para mapear 4 Gigabytes inteiros!
stack_bottom: .skip 16384
stack_top:

.section .rodata
gdt64:
    .quad 0
gdt64_code:
    .quad 0x00209A0000000000 # 64-bit Code
gdt64_data:
    .quad 0x0000920000000000 # 64-bit Data (Writable)
gdt64_pointer:
    .word gdt64_pointer - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    mov %ebx, %edi          # Salva o Multiboot Info no RDI (Argumento C de 64 bits)

    # 1. Mapeia PML4 -> PDP
    mov $pdp_table, %eax
    or $3, %eax
    mov %eax, pml4_table

    # 2. Mapeia PDP -> PD (Cria 4 entradas no PDP para cobrir 4GB)
    mov $pd_table, %eax
    or $3, %eax
    mov %eax, pdp_table             # 0GB - 1GB
    add $4096, %eax
    mov %eax, pdp_table + 8         # 1GB - 2GB
    add $4096, %eax
    mov %eax, pdp_table + 16        # 2GB - 3GB
    add $4096, %eax
    mov %eax, pdp_table + 24        # 3GB - 4GB

    # 3. Preenche as 2048 paginas de 2MB (Total = 4096 MB = 4GB de RAM mapeada!)
    mov $0, %ecx
map_pd:
    mov $0x200000, %eax     # Tamanho da Pagina: 2MB
    mul %ecx
    or $0x83, %eax          # Present + Writable + Huge Page
    mov %eax, pd_table(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx         # Mapeia 2048 vezes
    jne map_pd

    # Carrega as tabelas e ativa o Long Mode (64-bits)
    mov $pml4_table, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(1<<5), %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $(1<<8), %eax
    wrmsr

    mov %cr0, %eax
    or $(1<<31), %eax
    mov %eax, %cr0

    lgdt gdt64_pointer
    jmp $0x08, $long_mode_start

.code64
long_mode_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    call kernel_main
    cli
1:  hlt
    jmp 1b

.global load_idt
load_idt:
    lidt (%rdi)
    sti
    ret

.macro PUSH_ALL
    push %rax
    push %rcx
    push %rdx
    push %rbx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
.endm

.macro POP_ALL
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rbx
    pop %rdx
    pop %rcx
    pop %rax
.endm

.global isr0_stub
.extern exception0_handler
isr0_stub:
    PUSH_ALL
    call exception0_handler
    POP_ALL
    iretq

.global irq0_stub
.extern schedule
irq0_stub:
    PUSH_ALL
    mov %rsp, %rdi
    call schedule
    mov %rax, %rsp
    POP_ALL
    iretq

.global irq1_stub
.extern keyboard_handler_main
irq1_stub:
    PUSH_ALL
    call keyboard_handler_main
    POP_ALL
    iretq

.global irq12_stub
.extern mouse_handler_main
irq12_stub:
    PUSH_ALL
    call mouse_handler_main
    POP_ALL
    iretq

.section .data
.align 4
.global disk_start
.global disk_end
.global foto_bmp_start
.global foto_bmp_end

disk_start:
    .incbin "disk.img"
disk_end:

foto_bmp_start:
    .incbin "foto.bmp"
foto_bmp_end:
