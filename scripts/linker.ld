/* The bootloader will look at this image and start execution at the symbol
 *    designated as the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
 * kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	 * loaded at by the bootloader. VMA = 1M*/
	. = 1M;

	/* First put the multiboot header, as it is required to be put very early
	 * early in the image or the bootloader won't recognize the file format.
	 * Next we'll put the .text section. */
	/* kernel_start_addr and kernel_end_addr are used to track the location and the size of kernel
	 * Currently, because we open paging mode, so we set the kernel_start_addr to count from 0xC0000000*/
	kernel_start_addr = . + 0xC0000000;

	/* .init.text section's VMA is equal to its LMA, all are 1M */
	.init.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.init.text)
	}
	/* .init.data section's VMA is equal to its LMA, all are 1M + sizeof(.init.text) + alignment_gap */
	.init.data BLOCK(4K) : ALIGN(4K)
	{
		*(.init.data)
	}
	/* Now we set VMA of .text and all subsequent output sections count from 0xC0000000, 
	 * but still remains their LMA unchanged*/
	. += 0xC0000000;
	.text BLOCK(4K) : AT(ADDR(.text) - 0xC0000000) /* Set LMA of .text section. The following section is same. */
	ALIGN(4K)
	{
		*(.text)
	}

	/* Read-only data. */
	.rodata BLOCK(4K) : AT(ADDR(.rodata) - 0xC0000000)
	ALIGN(4K)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data BLOCK(4K) : AT(ADDR(.data) - 0xC0000000)
	ALIGN(4K)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) */
	.bss BLOCK(4K) : AT(ADDR(.bss) - 0xC0000000)
	ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	/* _prev_vma is aligned to next 4K boundary.
	 * We store location counter value before change it.*/
	_prev_vma = ALIGN(4K);
	/* We use 0x1000 as user process's entry point */
	. = 0x1000;
	.uinitcode : AT(_prev_vma - 0xC0000000) /* .uinitcode is located after .bss section */
	ALIGN(4K)
	{
		/* get LMA of uinit. */
		init_proc_start = _prev_vma - 0xC0000000;
		*(.uinitcode_text)
	}
	/* size of uinitcode. */
	init_proc_size = SIZEOF(.uinitcode);

	/* restore location counter value */
	. = _prev_vma + init_proc_size;
	kernel_end_addr = .;

	/* The compiler may produce other sections, by default it will put them in
	 * a segment with the same name. Simply add stuff here as needed. */
}
