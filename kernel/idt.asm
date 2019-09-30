;本文件提供中断处理程序中进入中断处理程序/保护现场过程和恢复现场过程，以及刷新idtr的过程
;注意：这些中断过程目前只适用于处理中断/异常，对于IDT中安装的陷阱门和任务门，可能不适用

section .text

global idt_flush		;void idt_flush(idt_ptr_t *)

;这个过程用于刷新IDTR，参数为指向待写入IDTR的数据起始地址，通过栈传递
idt_flush:
	mov eax, [esp+4]	;获取idt_ptr(在idt.c中定义)的地址
	lidt [eax]		;刷新IDTR

	ret


;定义两个特定的宏，用于构造中断处理程序的开头部分，在这里压入中断向量和错误代码
;这两个宏的参数均为中断向量

;没有错误代码的中断向量
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
	push 0			;压入一个伪造的错误代码，以便统一出栈
	push %1
	jmp isr_common_stub	;进入通用的中断处理程序部分
%endmacro

;有错误代码的中断向量
%macro ISR_ERRCODE 1
global isr%1
isr%1:
	push %1			;错误代码由CPU自动压入，所以只需要压入中断向量
	jmp isr_common_stub
%endmacro


;以下为isr0 - isr31的过程实现
ISR_NOERRCODE 0		;#DE
ISR_NOERRCODE 1		;#DB
ISR_NOERRCODE 2		;NMI
ISR_NOERRCODE 3		;#BP
ISR_NOERRCODE 4		;#OF
ISR_NOERRCODE 5		;#BR
ISR_NOERRCODE 6		;#UD
ISR_NOERRCODE 7		;#NM
ISR_ERRCODE 8		;#DF
ISR_NOERRCODE 9		;保留，在intel 386之后的处理器不产生该异常
ISR_ERRCODE 10		;#TS
ISR_ERRCODE 11		;#NP
ISR_ERRCODE 12		;#SS
ISR_ERRCODE 13		;#GP
ISR_ERRCODE 14		;#PF
ISR_NOERRCODE 15	;保留
ISR_NOERRCODE 16	;#MF
ISR_ERRCODE 17		;#AC，在intel 486中引入
ISR_NOERRCODE 18	;#MC，Introduced in the Pentium processor and enhanced in the P6 family processors.
ISR_NOERRCODE 19	;#XM，Introduced in the Pentium III processor.
ISR_NOERRCODE 20	;#VE，Can occur only on processors that support the 1-setting of the “EPT-violation #VE” VM-execution control.

ISR_NOERRCODE 21	;以下均为保留的中断向量
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; 下面实现服务中断向量0x20 - 0x2F的中断例程，这些中断向量由两片级联8259A PIC发起的
ISR_NOERRCODE 32	;映射到 master PIC的IR line 0
ISR_NOERRCODE 33	;映射到 master PIC的IR line 1
ISR_NOERRCODE 34	;映射到 master PIC的IR line 2
ISR_NOERRCODE 35	;映射到 master PIC的IR line 3
ISR_NOERRCODE 36	;映射到 master PIC的IR line 4
ISR_NOERRCODE 37	;映射到 master PIC的IR line 5
ISR_NOERRCODE 38	;映射到 master PIC的IR line 6
ISR_NOERRCODE 39	;映射到 master PIC的IR line 7
ISR_NOERRCODE 40	;映射到 slave PIC的IR line 0
ISR_NOERRCODE 41	;映射到 slave PIC的IR line 1
ISR_NOERRCODE 42	;映射到 slave PIC的IR line 2
ISR_NOERRCODE 43	;映射到 slave PIC的IR line 3
ISR_NOERRCODE 44	;映射到 slave PIC的IR line 4
ISR_NOERRCODE 45	;映射到 slave PIC的IR line 5
ISR_NOERRCODE 46	;映射到 slave PIC的IR line 6
ISR_NOERRCODE 47	;映射到 slave PIC的IR line 7

; 用于系统调用的中断例程，通过int n指令形式发起
ISR_NOERRCODE 255



;通用的保存现场/调用isr分发器/恢复现场部分
extern interrupt_handler_switcher	;idt.c中定义
isr_common_stub:
	push ds
	push es
	push fs
	push gs
	pushad		;将8个通用寄存器压入栈中
			;至此，中断现场信息全部保存，构成process.h中使用的trapframe_t结构体
	
	mov ax, 0x10	;切换到内核数据段，栈段由CPU决定是否切换
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	push esp	;压入目前的ESP值，该值指向上述中断现场信息的起始地址，作为参数来调用interrupt_handler_switcher
	call interrupt_handler_switcher
	add esp, 4	;清空参数

global trapret		;void trapret(void)
trapret:		;中断从这里返回
	popad		;恢复现场
	pop gs
	pop fs
	pop es
	pop ds

	add esp, 8	;清理栈中的中断向量和错误代码
	iretd


