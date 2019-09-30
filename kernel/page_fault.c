/*
 * 本文件给出处理#PF中断的实现。目前暂时只输出一些现场信息。
 */

#include "idt.h"
#include "terminal_io.h"

/* #PF的err_code与其他异常不同，下面是其中一部分 */
#define ERR_CODE_P	0x1
#define ERR_CODE_WR	0x2
#define ERR_CODE_US	0x4
#define ERR_CODE_RSVD	0x8
#define ERR_CODE_ID	0x10

void page_fault_handler(trapframe_t * info)
{
	/* cr2中保存有发生#PF的线性地址 */
	uint32_t cr2;
	asm volatile ("mov %%cr2, %0"
			: "=r" (cr2)
			:);
	
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "-----PAGE FAULT-----\n");
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "Linear Addr: %X, ", cr2);
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "CS:EIP: %X:%X\n", info->cs, info->eip);
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "Reason: \n");
	if(info->err_code & ERR_CODE_P)
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "A page-level protection violation.\n");
	else
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "A non-present page.\n");
	if(info->err_code & ERR_CODE_WR)
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "The access causing the fault was a write.\n");
	else
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "The access causing the fault was a read.\n");
	if(info->err_code & ERR_CODE_US)
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "A user-mode access caused the fault.\n");
	else
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "A supervisor-mode access caused the fault.\n");
	if(info->err_code & ERR_CODE_RSVD)
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "The fault was caused by a reserved bit set to 1 in some paging-structure entry.\n");
	if(info->err_code & ERR_CODE_ID)
		printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "The fault was caused by an instruction fetch.\n");


	while(1);
}
