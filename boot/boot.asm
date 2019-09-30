;Declare some constants for multiboot header

MBALIGN equ 1 << 0
MEMINFO equ 1 << 1
FLAGS	equ MBALIGN | MEMINFO
MAGIC	equ 0x1BADB002
CHECKSUM	equ -(MAGIC + FLAGS)



;Multiboot header
;Declare a multiboot header that marks the program as a kernel. These are magic
;values that are documented in the multiboot standard. The bootloader will
;search for this signature in the first 8 KiB of the kernel file, aligned at a
;32-bit boundary. The signature is in its own section so the header can be
;forced to be within the first 8 KiB of the kernel file.

section	.multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM



;The multiboot standard does not define the value of the stack pointer register
;(esp) and it is up to the kernel to provide a stack. This allocates room for a
;small stack by creating a symbol at the bottom of it, then allocating 16384
;bytes for it, and finally creating a symbol at the top. The stack grows
;downwards on x86. The stack on x86 must be 16-byte aligned according to the
;System V ABI standard and de-facto extensions. The compiler will assume the
;stack is properly aligned and failure to align the stack will result in
;undefined behavior.

section .init.data
align 16
stack_bottom:
resb 16384
stack_top:



;The linker script specifies _start as the entry point to the kernel and the
;bootloader will jump to this position once the kernel has been loaded.

section .init.text
global _start:function (_start.end - _start)	;Specify the entry point of our kernel to bootloader
extern kernel_init				;We need jump to kernel_init function in kernel.c
_start:
	mov esp, stack_top	;set stack
	push ebx		;pass pointer of Multiboot information struct
	call kernel_init

	cli
.hang:	hlt
	jmp .hang
.end:
