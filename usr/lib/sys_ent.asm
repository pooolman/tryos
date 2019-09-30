; 封装系统调用，便于用户从C中调用
%include "./inc/int_vectors.inc"
%include "./inc/syscall_numbers.inc"

section .text

; 用于封装系统调用的宏，遵循cdecl，参数为系统调用名
%macro SYSCALL 1
global %1
%1:
	mov eax, SYS_NUM_%1
	int SYSCALL_INT_VECTOR
	ret
%endmacro

; 目前已有的系统调用
SYSCALL debug
SYSCALL exec
SYSCALL fork
SYSCALL exit

;可能是nasm的一个bug，无法使用wait作为label
global _wait
_wait:
	mov eax, SYS_NUM_wait
	int SYSCALL_INT_VECTOR
	ret
	
SYSCALL getpid
SYSCALL dup
SYSCALL read
SYSCALL write
SYSCALL open
SYSCALL close
SYSCALL fstat
SYSCALL link
SYSCALL unlink
SYSCALL mkdir
SYSCALL mknod
SYSCALL chdir
SYSCALL pipe

