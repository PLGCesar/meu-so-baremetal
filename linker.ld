ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)
SECTIONS
{
    . = 1M;
    _kernel_start = .;

    .multiboot : ALIGN(4K) { KEEP(*(.multiboot)) }
    .text : ALIGN(4K) { *(.text .text.*) }
    .rodata : ALIGN(4K) { *(.rodata .rodata.*) }
    .data : ALIGN(4K) { *(.data .data.*) }
    .bss : ALIGN(4K) {
        *(COMMON)
        *(SORT_BY_ALIGNMENT(.bss*))
    }
    /DISCARD/ : { *(.comment) *(.note.gnu.build-id) *(.eh_frame) }

    _kernel_end = .;
}
