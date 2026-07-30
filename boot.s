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
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
	mov $stack_top, %esp
	push %ebx
	call kernel_main
	cli
1:	hlt
	jmp 1b

.global load_idt
load_idt:
    mov 4(%esp), %eax
    lidt (%eax)
    sti
    ret

.global irq0_stub
.extern schedule
irq0_stub:
    pusha
    cld
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x18, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    push %esp
    call schedule
    mov %eax, %esp

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

.global isr0_stub
.extern exception0_handler
isr0_stub:
    pusha
    cld
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x18, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    call exception0_handler
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

.global irq1_stub
.extern keyboard_handler_main
irq1_stub:
    pusha
    cld
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x18, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    call keyboard_handler_main
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

.global irq12_stub
.extern mouse_handler_main
irq12_stub:
    pusha
    cld
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x18, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    call mouse_handler_main
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

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
