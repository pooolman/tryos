#ifndef _INCLUDE_IDT_H_
#define _INCLUDE_IDT_H_

#include <stdint.h>

#define IDT_ENTRY_COUNT 256	//IDT条目个数

/* 一些中断向量号 */
#define DE_INT_VECTOR	0
#define DB_INT_VECTOR	1

#define BP_INT_VECTOR	3
#define OF_INT_VECTOR	4
#define BR_INT_VECTOR	5
#define UD_INT_VECTOR	6
#define NM_INT_VECTOR	7
#define DF_INT_VECTOR	8

#define TS_INT_VECTOR	10
#define NP_INT_VECTOR	11
#define SS_INT_VECTOR	12
#define GP_INT_VECTOR	13
#define PF_INT_VECTOR	14

#define MF_INT_VECTOR	16
#define AC_INT_VECTOR	17
#define MC_INT_VECTOR	18
#define XM_INT_VECTOR	19

#define SYSCALL_INT_VECTOR	255


/* Layout of the trap frame built on the stack by the
 * hardware and by idt.asm, and passed to interrupt
 * handler switcher. All padding member is useless. 
 */
typedef struct{
	// registers as pushed by pusha
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t _esp; // useless & ignored
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	
	// rest of trap frame
	uint16_t gs;
	uint16_t padding1;
	uint16_t fs;
	uint16_t padding2;
	uint16_t es;
	uint16_t padding3;
	uint16_t ds;
	uint16_t padding4;
	uint32_t trap_no;
	
	// below here defined by x86 hardware
	uint32_t err_code; // 错误代码(有中断错误代码则由CPU压入，若无也会手动压入一个0)
	uint32_t eip;
	uint16_t cs;
	uint16_t padding5;
	uint32_t eflags;
	
	// below here only when crossing rings, such as from user to kernel
	uint32_t esp;
	uint16_t ss;
	uint16_t padding6;
} trapframe_t;


typedef struct{
	uint16_t offset_low;		//中断例程在目标代码段内的低16位偏移，若为任务门则该值应置0
	uint16_t selector;		//中断例程所在目标代码段的选择子，若为任务门则为TSS选择子
	uint8_t always_zero;		//该值必须为0
	uint8_t attributes;		//描述符属性
	uint16_t offset_high;		//中断例程在目标代码段内的高16位偏移，若为任务门则该值应置0
}__attribute__((packed)) idt_entry_t;


typedef struct{
	uint16_t limit;			//IDT界限值
	uint32_t base;			//IDT线性基地址
}__attribute__((packed)) idt_ptr_t;


/* 指向处理具体中断的函数指针类型 */
typedef void (* interrupt_handler_t)(trapframe_t *);

/* 定义在idt.asm 中 */
/* 恢复中断现场 */
extern void trapret(void); 
/* 用于刷新IDTR的汇编过程*/
extern void idt_flush(idt_ptr_t *);

void init_idt(void);
void register_interrupt_handler(uint8_t int_vertor, interrupt_handler_t interrupt_handler);



#endif  //_INCLUDE_IDT_H_
