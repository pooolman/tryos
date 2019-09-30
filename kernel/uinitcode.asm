section .uinitcode_text
uinitcode:
	; simulates call to exec(path, argv)
	push argv	; right to left
	push path
	push 0		; fake return address
	mov eax, 1
	int 255
.1	jmp .1

path:
	db '/uinit', 0
argv:
	dd path
	dd 0	;NULL terminated argv array
