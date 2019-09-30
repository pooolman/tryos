; 本过程用于刷新GDTR，并根据GDT中的内容刷新所有段寄存器（CS/DS/ES/FS/GS/SS）
; 参数为待写入GDTR的内容的物理地址（32位），通过栈传递
global gdt_flush
section .text
gdt_flush:
	mov eax, [esp+4]		;获取参数
	lgdt [eax]			;刷新GDTR
	
	;以RPL=0/索引号为0x2查找GDT中的条目，分别加载到DS/ES/FS/GS/SS中
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	jmp 0x08:.flush			;清空流水线，以RPL=0/索引号为0x1查找GDT中的条目，以此刷新CS
.flush:	ret
