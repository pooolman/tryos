/*
 * 本文件提供一些debug工具函数的实现
 */
#include <stdint.h>
#include <stdarg.h>

#include "terminal_io.h"
#include "x86.h"
#include "string.h"
#include "process.h"

/*
 * 当assert失败时，可以调用这个函数输出一些简单的信息
 */
void panic_assert(const char * file_name, uint32_t line_number, const char * expression)
{
	asm volatile ("cli");

	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "ASSERT-FAILED(");
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "%s", expression);
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, ") at %s:%u\n", file_name, line_number);
	
	while(1);
}

/*
 * 当某任务无法完成时，可以调用这个函数输出一些简单的信息
 */
void panic(const char * file_name, uint32_t line_number, const char * msg)
{
	asm volatile ("cli");

	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "PANIC(");
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, "%s", msg);
	printk_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, ") at %s:%u\n", file_name, line_number);
	
	while(1);
}

/*
 * 向运行bochs的控制台输出字符串，字符串应该以'\0'字符结尾。
 */
void bochs_write_string(const char * string)
{
	while( *string )
	{
		outb(0xE9, (uint8_t)(*string));
		string++;
	}
}

/*
 * 向运行bochs的控制台输出格式化的字符串，工作于内核态。
 * 无背景/前景颜色等属性。使用方法类似于printk。
 */
void print_log(const char * format, ...)
{
	va_list ap;
	static char buff[1024];  //这里未检查溢出，下同 :(
	
	va_start(ap, format);
	vsprintf(buff, format, ap);
	va_end(ap);

	bochs_write_string(buff);
}

/*
 * 类似于print_log，可防止输出过程中被中断。不能在外部硬件中断处理程序或scheduler线程中使用。
 */
void iprint_log(const char * format, ...)
{
	va_list ap;
	static char buff[1024];
	
	va_start(ap, format);
	vsprintf(buff, format, ap);
	va_end(ap);

	pushcli();
	bochs_write_string(buff);
	popcli();
}
