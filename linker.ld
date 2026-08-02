ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)
SECTIONS
{
    . = 1M;
    _kernel_start = .;

    .multiboot : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(4K)
    {
        *(.text .text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(SORT_BY_ALIGNMENT(.bss*))
    }

    /* Otimização Híbrida: Descarte de assinaturas e debuffs de C que só gastam memória no binário final */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }

    _kernel_end = .;
}
