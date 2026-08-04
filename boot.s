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
.align 8
gdt64:
    .quad 0
    .quad 0x00209A0000000000
    .quad 0x0000920000000000
    .quad 0x00CFFA000000FFFF
    .quad 0x0000F20000000000
    .quad 0x0020FA0000000000
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
    or $7, %eax
    mov %eax, pml4_table

    mov $pd_table, %eax
    or $7, %eax
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
    or $0x187, %eax
    mov %eax, pd_table(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx
    jne map_pd

    mov $pml4_table, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(1<<5), %eax
    or $(1<<7), %eax
    or $(3<<9), %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $(1<<8), %eax
    wrmsr

    mov %cr0, %eax
    and $0xFFFFFFFB, %eax
    or $0x2, %eax
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

    mov $1, %eax
    cpuid
    and $0x18000000, %ecx
    cmp $0x18000000, %ecx
    jne skip_avx

    mov %cr4, %rax
    or $(1 << 18), %rax
    mov %rax, %cr4

    xor %ecx, %ecx
    mov $7, %eax
    xor %edx, %edx
    xsetbv
skip_avx:
    call kernel_main
    cli
1:  hlt
    jmp 1b

.global load_idt
load_idt:
    lidt (%rdi)
    ret

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
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi
    mov %rax, %rdi
    mov %rsp, %rbp
    and $-16, %rsp
    call sys_handler
    mov %rbp, %rsp
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
    push %rax; push %rcx; push %rdx; push %rbx; push %rbp
    push %rsi; push %rdi; push %r8; push %r9; push %r10
    push %r11; push %r12; push %r13; push %r14; push %r15
.endm

.macro POP_ALL
    pop %r15; pop %r14; pop %r13; pop %r12; pop %r11
    pop %r10; pop %r9; pop %r8; pop %rdi; pop %rsi
    pop %rbp; pop %rbx; pop %rdx; pop %rcx; pop %rax
.endm

.global default_irq_stub
default_irq_stub:
    PUSH_ALL
    mov $0x20, %al
    out %al, $0x20
    out %al, $0xA0
    POP_ALL
    iretq

.global isr0_stub
.extern exception0_handler
isr0_stub:
    PUSH_ALL
    mov %rsp, %rbp
    and $-16, %rsp
    call exception0_handler
    mov %rbp, %rsp
    POP_ALL
    iretq

.global isr8_stub
.extern exception8_handler
isr8_stub:
    PUSH_ALL
    mov 120(%rsp), %rdi
    mov %rsp, %rbp
    and $-16, %rsp
    call exception8_handler
    mov %rbp, %rsp
    POP_ALL
    add $8, %rsp
    iretq

.global isr13_stub
.extern exception13_handler
isr13_stub:
    PUSH_ALL
    mov 120(%rsp), %rdi
    mov %rsp, %rbp
    and $-16, %rsp
    call exception13_handler
    mov %rbp, %rsp
    POP_ALL
    add $8, %rsp
    iretq

.global isr14_stub
.extern exception14_handler
isr14_stub:
    PUSH_ALL
    mov 120(%rsp), %rdi
    mov %rsp, %rbp
    and $-16, %rsp
    call exception14_handler
    mov %rbp, %rsp
    POP_ALL
    add $8, %rsp
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
    mov %rsp, %rbp
    and $-16, %rsp
    call keyboard_handler_main
    mov %rbp, %rsp
    POP_ALL
    iretq

.global irq12_stub
.extern mouse_handler_main
irq12_stub:
    PUSH_ALL
    mov %rsp, %rbp
    and $-16, %rsp
    call mouse_handler_main
    mov %rbp, %rsp
    POP_ALL
    iretq

.section .data
.align 4
.global disk_start; .global disk_end
.global foto_bmp_start; .global foto_bmp_end
.global bgif_anim_start; .global bgif_anim_end
.global app_elf_start; .global app_elf_end
disk_start: .incbin "disk.img"; disk_end:
foto_bmp_start: .incbin "foto.bmp"; foto_bmp_end:
bgif_anim_start: .incbin "animacao.bgif"; bgif_anim_end:
app_elf_start: .incbin "app.elf"; app_elf_end:
