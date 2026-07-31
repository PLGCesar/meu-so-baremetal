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
pd_table: .skip 16384
stack_bottom: .skip 16384
stack_top:

.section .rodata
gdt64:
    .quad 0
gdt64_code:
    .quad 0x00209A0000000000
gdt64_data:
    .quad 0x0000920000000000
gdt64_pointer:
    .word gdt64_pointer - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    mov %ebx, %edi

    mov $pdp_table, %eax
    or $3, %eax
    mov %eax, pml4_table

    mov $pd_table, %eax
    or $3, %eax
    mov %eax, pdp_table
    add $4096, %eax
    mov %eax, pdp_table + 8
    add $4096, %eax
    mov %eax, pdp_table + 16
    add $4096, %eax
    mov %eax, pdp_table + 24

    mov $0, %ecx
map_pd:
    mov $0x200000, %eax
    mul %ecx
    or $0x83, %eax
    mov %eax, pd_table(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx
    jne map_pd

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

/* ENTRADA DA SYSCALL EM ASSEMBLY 64-BITS */
.global syscall_entry
.extern sys_handler
syscall_entry:
    push %rcx
    push %r11
    push %r15
    push %r14
    push %r13
    push %r12
    push %r10
    push %rbx
    push %rbp

    mov %rax, %rdi
    mov %rsi, %rsi
    mov %rdx, %rdx
    mov %r10, %rcx

    call sys_handler

    pop %rbp
    pop %rbx
    pop %r10
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    pop %r11
    pop %rcx
    sysretq

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
