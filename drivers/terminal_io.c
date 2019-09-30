/*
 * 本文件中提供用于在内核态下向屏幕输出格式化内容的函数实现
 */

#include "terminal_io.h"
#include "process.h"


/*
 * 用于向屏幕格式化输出，工作于内核态，使用默认属性（黑色背景,亮白色前景，不闪烁）输出字符串。
 * 使用方法类似于stdio中的printf函数
 */
void printk(const char * format, ...)
{
	va_list ap;
	static char buff[1024]; //使用static变量以节省内核栈空间，这里未检查溢出，下同 :(
	
	va_start(ap, format);
	vsprintf(buff, format, ap);
	va_end(ap);

	console_write_string(buff);
}

/*
 * 等同于printk函数，除了需要指定属性（背景色，前景色，是否闪烁）
 */
void printk_color(vga_color_t fg, vga_color_t bg, const char * format, ...)
{
	va_list ap;
	static char buff[1024];
	
	va_start(ap, format);
	vsprintf(buff, format, ap);
	va_end(ap);

	console_write_string_color(buff, fg, bg);
}


/*
 * 等同于printk，在进程可调度时可以使用，可防止输出过程被中断，在外部硬件中断处理程序或者scheduler线程中不能使用
 */
void iprintk(const char * format, ...)
{
	va_list ap;
	static char buff[1024]; //使用static变量以节省内核栈空间
	
	va_start(ap, format);
	vsprintf(buff, format, ap);
	va_end(ap);

	pushcli();
	console_write_string(buff);
	popcli();
}


/*
 * 等同于printk_color，在进程可调度时可以使用，可防止输出过程被中断，在外部硬件中断处理程序或者scheduler线程中不能使用
 */
void iprintk_color(vga_color_t fg, vga_color_t bg, const char * format, ...)
{
	va_list ap;
	static char buff[1024];
	
	va_start(ap, format);
	vsprintf(buff, format, ap);
	va_end(ap);

	pushcli();
	console_write_string_color(buff, fg, bg);
	popcli();
}
