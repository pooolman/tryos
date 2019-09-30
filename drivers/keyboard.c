/*
 * 简单的PS/2键盘支持
 */
#include <stdint.h>

#include "debug.h"
#include "idt.h"
#include "terminal_io.h"
#include "x86.h"
#include "8259A.h"
#include "tty.h"

/* PS/2控制器状态/命令端口，字节大小，
 * 读取时为状态端口，写入时为命令端口 */
#define KBD_STAT_PORT	0x64
/* PS/2控制器数据端口，字节大小 */
#define KBD_DATA_PORT	0x60
/* PS/2控制器标志，表明output buffer中数据已满 */
#define KBD_STAT_FL_DIB	0x01

static void kbd_handler(trapframe_t * tf)
{
	uint8_t sc;

	/* 是否有数据可读 */
	if( ! (inb(KBD_STAT_PORT) & KBD_STAT_FL_DIB))
		return;
	
	/* 有则读取scan code */
	sc = inb(KBD_DATA_PORT);

	/* 交给tty设备处理 */
	tty_handler(sc);
}

void init_kbd(void)
{
	/* 注册键盘中断的处理函数 */
	register_interrupt_handler(IRQ1_INT_VECTOR, kbd_handler);
	/* 使键盘中断能够通过8259A；PS/2键盘对应IRQ 1 */
	enable_IRline(1);
}
