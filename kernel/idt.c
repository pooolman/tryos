/*
 * 本文件提供与IDT相关的函数实现
 */

#include "debug.h"
#include "idt.h"
#include "string.h"
#include "terminal_io.h"
#include "process.h"
#include "8259A.h"
#include "syscall.h"

//static idt_entry_t idt_entries[IDT_ENTRY_COUNT]; //IDT

static idt_ptr_t idt_ptr; //IDTR

static interrupt_handler_t interrupt_handler_table[IDT_ENTRY_COUNT]; //中断服务函数表

/* 中断服务例程的汇编入口 */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* 以下为用于服务两片级联8259A PIC的中断服务例程 */
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);

/* 处理系统调用的中断服务例程 */
extern void isr255(void);

/*
 * 根据中断时的现场信息，将不同中断分发给不同处理函数。
 * 汇编过程将调用这个函数，实现C语言层次的中断处理
 */
void interrupt_handler_switcher(trapframe_t * tf)
{
	if(tf->trap_no <= 31) //trap_no为无符号数，必定大于等于0
	{
		/* 0～31之间的中断和异常 */
		if(interrupt_handler_table[tf->trap_no])
			interrupt_handler_table[tf->trap_no](tf);
		else
		{
			printk("Unexcepted interrupt %u eip %X errcode %X\n", tf->trap_no, tf->eip, tf->err_code);
			PANIC("interrupt_handler_switcher: unexcepted interrupt");
		}
	}
	else if(tf->trap_no >= 32 && tf->trap_no <= 47)
	{
		/* 8259A发起的中断 */
		/* 没有更新PCB.tf的值！*/
		switch(tf->trap_no)
		{
			case IRQ0_INT_VECTOR:
			case IRQ1_INT_VECTOR:
			case IRQ14_INT_VECTOR:
				/* 发送EOI */
				send_EOI((uint8_t)tf->trap_no);

				if(interrupt_handler_table[tf->trap_no])
					interrupt_handler_table[tf->trap_no](tf);
				else
					PANIC("interrupt_handler_switcher: timer/ide interrupt unhandled");
				break;
			default:
				//printk("Unexcepted IRQ %u eip %X errcode %X\n", tf->trap_no, tf->eip, tf->err_code);
				print_log("Unexcepted IRQ %u eip %X errcode %X\n", tf->trap_no, tf->eip, tf->err_code);
				break;
		}
	}
	else if(tf->trap_no == SYSCALL_INT_VECTOR)
	{
		/* 系统调用 */
		/* 需要更新trapframe在内核栈中的位置 */
		cpu.cur_proc->tf = tf;
		syscall();
	}
	else
	{
		printk("Unexcepted interrupt %u eip %X errcode %X\n", tf->trap_no, tf->eip, tf->err_code);
		PANIC("interrupt_handler_switcher: unexcepted interrupt");
	}

}


/*
 * 安装与特定中断向量相关的IDT条目
 */
static void install_idt_entry(uint8_t int_vector, uint32_t offset, uint16_t selector, uint8_t attributes)
{
	cpu.IDT[int_vector].offset_low = (uint16_t)offset;
	cpu.IDT[int_vector].offset_high = (uint16_t)(offset >> 16);
	cpu.IDT[int_vector].selector = selector;
	cpu.IDT[int_vector].always_zero = 0;
	cpu.IDT[int_vector].attributes = attributes;
}

/*
 * 建立IDT，刷新IDTR，根据需要注册处理特定中断的函数
 */
void init_idt(void)
{
	/* 设置将写入IDTR的值 */
	idt_ptr.limit = (uint16_t)(IDT_ENTRY_COUNT * sizeof(idt_entry_t) - 1);
	idt_ptr.base = (uint32_t)cpu.IDT;

	/* 安装必要的IDT条目 */
	memset(cpu.IDT, 0, sizeof(cpu.IDT)); //先清零（P位为0），如果遇到未处理的中断，CPU将发出异常
	memset(interrupt_handler_table, 0, sizeof(interrupt_handler_table));  //也对中断函数表清零
	
	/* 0 ～20号中断和异常向量对应的idt条目，中断门，DPL均为0 */
	install_idt_entry(0, (uint32_t)isr0, 0x08, 0x8E);
	install_idt_entry(1, (uint32_t)isr1, 0x08, 0x8E);
	install_idt_entry(2, (uint32_t)isr2, 0x08, 0x8E);
	install_idt_entry(3, (uint32_t)isr3, 0x08, 0x8E);
	install_idt_entry(4, (uint32_t)isr4, 0x08, 0x8E);
	install_idt_entry(5, (uint32_t)isr5, 0x08, 0x8E);
	install_idt_entry(6, (uint32_t)isr6, 0x08, 0x8E);
	install_idt_entry(7, (uint32_t)isr7, 0x08, 0x8E);
	install_idt_entry(8, (uint32_t)isr8, 0x08, 0x8E);
	install_idt_entry(9, (uint32_t)isr9, 0x08, 0x8E);
	install_idt_entry(10, (uint32_t)isr10, 0x08, 0x8E);
	install_idt_entry(11, (uint32_t)isr11, 0x08, 0x8E);
	install_idt_entry(12, (uint32_t)isr12, 0x08, 0x8E);
	install_idt_entry(13, (uint32_t)isr13, 0x08, 0x8E);
	install_idt_entry(14, (uint32_t)isr14, 0x08, 0x8E);
	install_idt_entry(15, (uint32_t)isr15, 0x08, 0x8E);
	install_idt_entry(16, (uint32_t)isr16, 0x08, 0x8E);
	install_idt_entry(17, (uint32_t)isr17, 0x08, 0x8E);
	install_idt_entry(18, (uint32_t)isr18, 0x08, 0x8E);
	install_idt_entry(19, (uint32_t)isr19, 0x08, 0x8E);
	install_idt_entry(20, (uint32_t)isr20, 0x08, 0x8E);

	/* 以下中断向量未被intel定义，不要发起这些中断，否则可能不能正常工作 */
	/* 中断门，DPL均为0 */
	install_idt_entry(21, (uint32_t)isr21, 0x08, 0x8E); 
	install_idt_entry(22, (uint32_t)isr22, 0x08, 0x8E);
	install_idt_entry(23, (uint32_t)isr23, 0x08, 0x8E);
	install_idt_entry(24, (uint32_t)isr24, 0x08, 0x8E);
	install_idt_entry(25, (uint32_t)isr25, 0x08, 0x8E);
	install_idt_entry(26, (uint32_t)isr26, 0x08, 0x8E);
	install_idt_entry(27, (uint32_t)isr27, 0x08, 0x8E);
	install_idt_entry(28, (uint32_t)isr28, 0x08, 0x8E);
	install_idt_entry(29, (uint32_t)isr29, 0x08, 0x8E);
	install_idt_entry(30, (uint32_t)isr30, 0x08, 0x8E);
	install_idt_entry(31, (uint32_t)isr31, 0x08, 0x8E);


	/* 在这里添加自定义的中断门以及注册具体的中断处理函数 */

	/* 安装针对8259A所发起中断的中断门，DPL为0 */
	install_idt_entry(32, (uint32_t)isr32, 0x08, 0x8E);
	install_idt_entry(33, (uint32_t)isr33, 0x08, 0x8E);
	install_idt_entry(34, (uint32_t)isr34, 0x08, 0x8E);
	install_idt_entry(35, (uint32_t)isr35, 0x08, 0x8E);
	install_idt_entry(36, (uint32_t)isr36, 0x08, 0x8E);
	install_idt_entry(37, (uint32_t)isr37, 0x08, 0x8E);
	install_idt_entry(38, (uint32_t)isr38, 0x08, 0x8E);
	install_idt_entry(39, (uint32_t)isr39, 0x08, 0x8E);
	install_idt_entry(40, (uint32_t)isr40, 0x08, 0x8E);
	install_idt_entry(41, (uint32_t)isr41, 0x08, 0x8E);
	install_idt_entry(42, (uint32_t)isr42, 0x08, 0x8E);
	install_idt_entry(43, (uint32_t)isr43, 0x08, 0x8E);
	install_idt_entry(44, (uint32_t)isr44, 0x08, 0x8E);
	install_idt_entry(45, (uint32_t)isr45, 0x08, 0x8E);
	install_idt_entry(46, (uint32_t)isr46, 0x08, 0x8E);
	install_idt_entry(47, (uint32_t)isr47, 0x08, 0x8E);
	
	/* 用于系统调用的陷阱门，DPL为3，RPL为0(
	 * 因为进入中断或者异常处理例程时不检查RPL) */
	install_idt_entry(SYSCALL_INT_VECTOR, (uint32_t)isr255, 0x08, 0xEF);

	/* 刷新IDTR */
	idt_flush(&idt_ptr);
}

/*
 * 在中断服务函数表中注册处理特定中断的函数
 */
void register_interrupt_handler(uint8_t int_vector, interrupt_handler_t interrupt_handler)
{
	interrupt_handler_table[int_vector] = interrupt_handler;
}
