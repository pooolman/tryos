;
; 本文件提供与process相关的汇编例程
;

section .text
;
; 切换内核线程的上下文
; void swtch(context_t ** from, context_t * to)
;
; H
; |eip
; |eflags
; |edi
; |esi
; |ebp
; |ebx <-- *from
; L

global swtch
swtch:
	; 取参数
	mov eax, [esp + 4]	; from
	mov ecx, [esp + 8]	; to

	; 保存当前context到当前内核线程栈中
	; eip在call swtch时已经压入栈中了
	pushfd
	push edi
	push esi
	push ebp
	push ebx

	; 更新*from，并切换到新内核线程的内核上下文
	mov [eax], esp
	mov esp, ecx

	; 恢复新内核线程的内核上下文
	pop ebx
	pop ebp
	pop esi
	pop edi
	popfd
	ret
