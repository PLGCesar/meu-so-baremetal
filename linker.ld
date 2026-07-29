ENTRY(_start)

SECTIONS
{
	. = 1M;
	_kernel_start = .;

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	/* Marca o fim exato do Kernel na RAM para o nosso Alocador de Memória usar */
	_kernel_end = .;
}
