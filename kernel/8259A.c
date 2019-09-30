/*
 * 本文件提供初始化两个级联的8259A PIC的函数实现
 */

#include "x86.h"
#include "8259A.h"

/* 两个PIC的端口地址 */
#define PIC1_COMMAND	0x20 //master PIC的控制端口
#define PIC1_DATA	0x21 //master PIC的数据端口
#define PIC2_COMMAND	0xA0 //slave PIC的控制端口
#define PIC2_DATA	0xA1 //slave PIC的数据端口

/* 
 * PIC1所映射的中断向量为 0x20 - 0x27，PIC2所映射的中断向量为 0x28 - 0x2F，其中0x22中断向量
 * 对应的IR line用于连接PIC2，所以这个中断向量无意义
 */
#define PIC1_INT_VECTOR_OFFSET 0x20
#define PIC2_INT_VECTOR_OFFSET 0x28

/*
 * 初始化两个8259A PIC，包括：
 * 	配置级联方式
 * 	设置映射的中断向量
 * 	设置IMR
 */
void init_8259A(void)
{
	/* 发送ICW1给PIC1和PIC2，开始初始化；发出这个命令后，8259A期待连续接收 ICW2/ICW3/ICW4 这三个命令 */
	outb(PIC1_COMMAND, 0x11);
	outb(PIC2_COMMAND, 0x11);
	/* 发送ICW2给PIC1和PIC2，修改IR line对应的中断向量 */
	outb(PIC1_DATA, PIC1_INT_VECTOR_OFFSET);
	outb(PIC2_DATA, PIC2_INT_VECTOR_OFFSET);
	/* 发送ICW3给PIC1和PIC2，设置两个PIC的连接方式:PIC2的INT引脚连接到PIC1的IR line 2上 */
	outb(PIC1_DATA, 0x04); //0x04 => 0100, bit2 (IR line 2)
	outb(PIC2_DATA, 0x02); //010 => IR line 2
	/* 发送ICW4给PIC1和PIC2，设置为8086模式 */
	outb(PIC1_DATA, 0x01);
	outb(PIC2_DATA, 0x01);

	/* 设置PIC1和PIC2的IMR */
	outb(PIC1_DATA, 0xFB); //1111 1011，目前只打开IR line 2
	outb(PIC2_DATA, 0xFF); //1111 1111，目前全部关闭
}

/*
 * 设置IR line在IMR寄存器中对应的位，即忽略该IR line上的中断请求
 * IR line值为0-15
 */
void inhibit_IRline(uint8_t IRline)
{
	if(IRline > 7)
	{
		/* 该IR line在slave PIC上 */
		uint8_t imr = inb(PIC2_DATA);
		outb(PIC2_DATA, imr |(1 << (IRline - 8)));
	}
	else
	{
		/* 该IR line在master PIC上 */
		uint8_t imr = inb(PIC1_DATA);
		outb(PIC1_DATA, imr | (1 << IRline));
	}
}

/*
 * 清除IR line在IMR寄存器中对应的位，即接受该IR line上的中断请求
 * IR line值为0-15
 */
void enable_IRline(uint8_t IRline)
{
	if(IRline > 7)
	{
		uint8_t imr = inb(PIC2_DATA);
		imr = imr & ~(1 << (IRline - 8));
		outb(PIC2_DATA, imr);
	}
	else
	{
		uint8_t imr = inb(PIC1_DATA);
		imr = imr & ~(1 << IRline);
		outb(PIC1_DATA, imr);
	}
}

/*
 * 向master PIC发送EOI命令，如果中断在slave PIC上发起，则同时向slave PIC发送EOI命令
 * int_vector: 中断向量，其值应该在master/slave PIC所处理的中断向量范围内
 */
void send_EOI(uint8_t int_vector)
{
	outb(PIC1_COMMAND, 0x20); //向master PIC发送EOI
	if(int_vector >= PIC2_INT_VECTOR_OFFSET) //中断从slave PIC上发起
		outb(PIC2_COMMAND, 0x20); //向slave PIC发送EOI
}
