/*
 * 本文件提供配置8253/8254 PIT的函数实现
 */

#include "8253pit.h"
#include "8259A.h"
#include "x86.h"

/*
 * 设置8253 PIT的工作模式，包括指定工作频率（由参数指定）,
 * 即每秒大约产生多少次中断；同时设置8259A PIC以允许将该中断
 * 发给CPU
 * 注意：工作频率建议为偶数值，范围必须在19hz-1193180hz之间（包括两端的值）
 */
void init_8253pit(uint32_t hz)
{
	/* 8253/8254 PIT中的振荡器频率大约为1193180 Hz */
	uint32_t counter = 1193180 / hz;
	
	/* 设置工作模式 */

	outb(0x43, 0x36); //命令端口为0x43，0x36的含义参考手册

	/* 只能写入16位的值 */
	uint8_t lobyte = (uint8_t)counter;
	uint8_t hibyte = (uint8_t)(counter >> 8);
	outb(0x40, lobyte); //0x40为channel 0的数据端口
	outb(0x40, hibyte);


	/* 设置结束，修改8259A PIC的IMR以允许中断通过 */
	enable_IRline(0); //8253/8254 PIT连接到IR line 0上
}
