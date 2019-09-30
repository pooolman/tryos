#include <stdint.h>

#include "debug.h"
#include "keyboard.h"
#include "parameters.h"
#include "console.h"
#include "process.h"
#include "terminal_io.h"
#include "inode.h"


/* tty定义 */
typedef struct {
	uint8_t input_buf[TTY_INPUT_BUF_SIZE]; //输入缓冲区
	uint32_t r; //读开始位置
	uint32_t w; //读截至位置，编辑开始位置
	uint32_t e; //新数据写入位置
} tty_t;

/* tty设备 */
tty_t tty;


/*
 * 将扫描码转换为对应按键和动作的组合
 * sc: 扫描码
 *
 * 如果当前sc或者和之前输入的sc结合起来，
 * 能够表达出一次按键动作的含义，那么将返回
 * 一个表示该按键动作含义的值，否则返回0；
 *
 * 注意：
 * 该函数仅仅处理了数字、字母、部分shift组合键、
 * capslock键等常用键；与alt/ctrl有关的键以及
 * 一些其他键均未提供支持。
 */
uint8_t map_sc(uint8_t sc)
{
	static uint32_t flags = 0; //目前的按键状态
	uint8_t data;
	
	if(sc == 0xE0)
	{
		/* 0xE0开头 */
		flags |= FL_E0ESC;
		data = 0;
	}
	else
	{
		/* 0xE1开头以及PrintScreen键按照普通的按键处理 */
		if(sc & 0x80)
		{
			/* break code可能导致状态改变，但目前只处理下面两种情况 */
			switch(sc)
			{
				case 0xAA:
					flags &= ~FL_SHIFT_L;
					break;
				case 0xB6:
					flags &= ~FL_SHIFT_R;
					break;
			}
			data = 0;
		}
		else
		{
			/* make code */

			if(flags & (FL_SHIFT_L | FL_SHIFT_R))
			{
				/* 已经按下了shift键 */
				data = shift_map[sc];
				if(flags & FL_CAPS_LOCK)
					if(data >= 'A' && data <= 'Z')
						data += 'a' - 'A';
			}
			else
			{
				/* 没有按下shift键 */

				if(flags & FL_E0ESC)
				{
					/* 但是是0xE0开头的扫描码 */
					data = e0_map[sc];
					flags &= ~FL_E0ESC;
				}
				else
				{
					/* 其余的扫描码都做如下处理 */
					data = normal_map[sc];
					switch(data)
					{
						case KEY_SHIFT_L:
							flags |= FL_SHIFT_L;
							data = 0;
							break;
						case KEY_SHIFT_R:
							flags |= FL_SHIFT_R;
							data = 0;
							break;
						case KEY_CAPS_LOCK:
							if(flags & FL_CAPS_LOCK)
								flags &= ~FL_CAPS_LOCK;
							else
								flags |= FL_CAPS_LOCK;
							data = 0;
							break;
						default:
							if(flags & FL_CAPS_LOCK)
								if(data >= 'a' && data <= 'z')
									data -= 'a' - 'A';
					}
				}
			}
		}
	}
	return data;
}

/*
 * 对键盘输入数据做出响应
 * sc: 扫描码；
 *
 * 处理从键盘输入的数据，工作包括写入缓冲区使之对进程可读，
 * 回显在屏幕上(使用默认属性)，唤醒进程；
 *
 * 注意：
 * 该函数要求输入缓冲区长度应该是2的次幂
 */
void tty_handler(uint8_t sc)
{
	uint8_t data;
	
	/* 转换扫描码 */
	data = map_sc(sc);
	switch(data)
	{
		case 0:
			break;
		case KEY_BACKSPACE:
			if(tty.e != tty.w)
			{
				/* 有可编辑的内容，则回退上次的处理 */
				tty.e--;
				console_write_char('\b');
				console_write_char(' ');
				console_write_char('\b');
			}
			break;
		default:
			/* 暂时不处理更多的特殊按键，只要缓冲区未满，直接写入 */
			if((tty.r <= tty.e && tty.e - tty.r < TTY_INPUT_BUF_SIZE) ||
					(tty.r > tty.e && tty.e % TTY_INPUT_BUF_SIZE < tty.r % TTY_INPUT_BUF_SIZE))
			{
				/* 缓冲区未满 */
				/* 转换后写入缓冲区，并在屏幕上回显 */
				if(data == KEY_ENTER)
					data = '\n';
				else if(data == KEY_TAB)
					data = '\t';
				tty.input_buf[tty.e % TTY_INPUT_BUF_SIZE] = data;
				tty.e++;
				console_write_char(data);
				
				/* 如果已经输入一行或者缓冲区已满，则唤醒等待的进程 */
				if(data == '\n' || tty.e == tty.r + TTY_INPUT_BUF_SIZE)
				{
					tty.w = tty.e;
					wakeup_noint(&tty);
				}
			}
			/* 如果缓冲区已满则忽略这次的按键值 */
			break;
	}
}

/*
 * 从TTY设备读取数据
 * dst: 目标缓冲区；
 * n: 所需要读取的字节数；
 *
 * 从TTY设备的输入缓冲区读取一行（在'\n'处结束，包括'\n'）
 * 或者n个字节的数据（以先满足的条件为准）写入dst处，
 * 没有数据可读时会睡眠下去直到被唤醒，
 * 成功返回实际读取的字节数，没有失败情况；
 *
 * 注意：返回值为有符号，而n为无符号参数
 */
int32_t tty_read(void * dst, uint32_t n)
{
	uint32_t old_n;
	uint8_t data;
	
	old_n = n;

	pushcli();
	while(n > 0)
	{
		/* 没有数据可读则睡眠下去 */
		while(tty.r == tty.w)
			sleep(&tty);
		
		/* 否则写入dst处 */
		data = tty.input_buf[tty.r % TTY_INPUT_BUF_SIZE];
		*((uint8_t *)dst) = data;
		tty.r++;
		n--;
		dst++;

		/* 输入满一行就退出 */
		if(data == '\n')
			break;
	}
	popcli();

	return (int32_t)(old_n - n);
}

/*
 * 向TTY设备写入数据
 * src: 源缓冲区；
 * n: 所需要写入的字节数；
 *
 * 向屏幕当前光标处写入从src开始的n个字节的数据，
 * 使用默认属性（无闪烁、白色前景、黑色背景），
 * 成功返回n，没有失败情况；
 *
 * 注意：返回值为有符号，而n为无符号参数
 */
int32_t tty_write(void * src, uint32_t n)
{
	uint32_t old_n;

	old_n = n;

	pushcli();
	while(n > 0)
	{
		console_write_char(*((uint8_t *)src));
		src++;
		n--;
	}
	popcli();
	return (int32_t)old_n;
}

/*
 * 初始化tty对应设备表的表项
 */
void init_tty(void)
{
	chr_dev_opts_table[TTY_MAJOR_NUM].read = tty_read;
	chr_dev_opts_table[TTY_MAJOR_NUM].write = tty_write;
}

