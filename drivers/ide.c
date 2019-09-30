/*
 * 本文件提供一个简单的ide磁盘驱动程序实现。
 * 注意：本驱动程序中并未考虑现实硬件的速度和出错的可能
 */

#include <stdint.h>
#include <stddef.h>
#include "debug.h"
#include "ide.h"
#include "buf_cache.h"
#include "x86.h"
#include "8259A.h"
#include "process.h"
#include "idt.h"

#include "terminal_io.h"


/* some ide ports which related with one or two registers */
#define IDE_DATA_REG		0x1F0
#define IDE_ERR_REG		0x1F1
#define IDE_FEATURE_REG		0x1F1
#define IDE_SECTOR_COUNT_REG	0x1F2
#define	IDE_LBAlo_REG		0x1F3
#define IDE_LBAmid_REG		0x1F4
#define IDE_LBAhi_REG		0x1F5
#define IDE_DRIVE_HEAD_REG	0x1F6
#define IDE_STATUS_REG		0x1F7
#define IDE_CMD_REG		0x1F7
/* 在bochsrc文件中，针对ata0默认生成的这个端口起始地址是0x3F0，但是如果在程序中使用0x3F6也是可以的 */
#define IDE_CTRL_ALT_STATUS_REG	0x3F6
#define	IDE_CTRL_DEV_CONTROL_REG	0x3F6

/* some flags in ide status register and alternate status register */
#define IDE_STATUS_BSY		0x80	//Indicates the drive is preparing to send/receive data (wait for it to clear).
#define IDE_STATUS_DRDY		0x40	//Bit is clear when drive is spun down, or after an error. Set otherwise.
#define IDE_STATUS_DF		0x20	//Drive Fault Error.
#define IDE_STATUS_ERR		0x01	//Indicates an error occurred.

/* commands used in ide command register */
#define IDE_CMD_READ		0x20	//read command
#define IDE_CMD_WRITE		0x30	//write command

/* 硬盘请求队列头部 */
static buf_t * ide_queue;

/*
 * 轮询ide drive的状态寄存器，看其是否UN-BSY且RDY；如果需要检查是否出错且发现出错，则返回0，其余情况返回1。
 */
static uint32_t wait_ide(uint32_t need_check_err)
{
	uint8_t r;
	/* 在bochs中测试时，似乎不应该检测IDE_STATUS_DRDY */
	/*
	while(((r = inb(IDE_STATUS_REG)) & (IDE_STATUS_BSY | IDE_STATUS_DRDY)) != IDE_STATUS_DRDY)
		;
		*/
	while((r = inb(IDE_STATUS_REG)) & IDE_STATUS_BSY)
		;
	if(need_check_err && (r & (IDE_STATUS_DF | IDE_STATUS_ERR)) != 0)
		return 0;
	return 1;
}


/*
 * 将请求提交给硬件
 */
static void start_ide_request(buf_t * buf)
{
	if(buf == NULL || buf->dev < 0)
		PANIC("start_ide_request: illegal request");

	wait_ide(0); //等待硬件做好准备
	outb(IDE_CTRL_DEV_CONTROL_REG, 0); //配置硬件在完成请求后产生中断
	outb(IDE_SECTOR_COUNT_REG, 1); //一次处理一个sector
	outb(IDE_LBAlo_REG, buf->sector & 0xFF); //依次写入LBA28的低24位
	outb(IDE_LBAmid_REG, (buf->sector >> 8) & 0xFF);
	outb(IDE_LBAhi_REG, (buf->sector >> 16) & 0xFF);
	outb(IDE_DRIVE_HEAD_REG, 0xe0 | ((buf->sector >> 24) & 0xF)); //使用LBA28，选择master驱动，并给出LBA28的高4位

	if(buf->flags & BUF_DIRTY) //如果是DIRTY的则写入，否则读取
	{
		outb(IDE_CMD_REG, IDE_CMD_WRITE);
		outsl(IDE_DATA_REG, buf->data, sizeof(buf->data)/4);
	}
	else
		outb(IDE_CMD_REG, IDE_CMD_READ);
}


/*
 * ide中断处理
 */
static void ide_handler(trapframe_t * tf)
{
	/* 由于CPU处理该中断时自动关闭了中断，所以不再调用pushcli/popcli */
	buf_t * buf;
	
	/* 检查是否有supurious irq */
	if((buf = ide_queue) == NULL)
		PANIC("ide_handler: maybe a supurious irq ?");

	/* 移除队列头部的请求 */
	ide_queue = ide_queue->qnext;

	/* 如果buf是UN-DIRTY，说明之前发起的是读请求，此时还需要从硬件读出数据到buf->data中 */
	if( ! (buf->flags & BUF_DIRTY))
	{
		if(wait_ide(1) > 0)
			insl(IDE_DATA_REG, buf->data, sizeof(buf->data)/4);
		else
			PANIC("ide_handler: maybe a disk error ?");
	}

	/* 此时buf为VALID且UN-DIRTY的 */
	buf->flags |= BUF_VALID;
	buf->flags &= ~BUF_DIRTY;
	/* 唤醒在等待这个buf的进程 */
	wakeup_noint(buf);

	/* 如果队列中还有请求未完成，那么现在就开始 */
	if(ide_queue)
		start_ide_request(ide_queue);

}


/*
 * IDE硬盘初始化，配置PIC以允许相关中断通过，注册中断处理函数，且等待驱动准备好
 */
void init_ide(void)
{
	enable_IRline(IRQ14_INT_VECTOR - IRQ0_INT_VECTOR);
	register_interrupt_handler(IRQ14_INT_VECTOR, ide_handler);
	wait_ide(0);
}


/*
 * 同步buf和磁盘，同步之后的buf将是VALID且UN-DIRTY，注意buf必须是BUSY的
 */
void sync_ide(buf_t * buf)
{
	buf_t ** bpp;

	if( ! (buf->flags & BUF_BUSY))
		PANIC("sync_ide: no process has owned this buf");
	if((buf->flags & (BUF_VALID | BUF_DIRTY)) == BUF_VALID)
		PANIC("sync_ide: nothing to do");
	
	pushcli(); //保证同时只有一个进程能够访问ide_queue

	/* 将buf添加到ide_queue的尾部 */
	for(bpp = &ide_queue; *bpp != NULL; bpp = &((*bpp)->qnext))
		;
	*bpp = buf;
	buf->qnext = NULL;

	/* 如果队列中仅当前一个请求 */
	if(ide_queue == buf)
		start_ide_request(buf);
	
	/* 轮询buf是否VALID且UN-DIRTY */
	while((buf->flags & (BUF_VALID | BUF_DIRTY)) != BUF_VALID)
		sleep(buf);

	popcli(); //恢复原来的中断状态
}

