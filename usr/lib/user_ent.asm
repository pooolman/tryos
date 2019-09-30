; 用户程序的进入/退出点
%include "./inc/int_vectors.inc"
%include "./inc/syscall_numbers.inc"

; 暂时没有完成退出部分的处理
section .text
global _ustart ; 用户程序的入口点
extern main
_ustart:
	call main
	push eax	; 将main函数的返回值传递给exit系统调用
	push 0x0	; 假的返回地址
	mov eax, SYS_NUM_exit	; 发起exit调用
	int SYSCALL_INT_VECTOR
	ret		; 如果执行到了这里，则让其PANIC
