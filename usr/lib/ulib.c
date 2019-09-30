/*
 * 用户程序可以使用的库函数
 */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "string.h"
#include "sys.h"
#include "ulib.h"

/*
 * 格式化输出到bochs控制台。
 * 使用方法类似于iprint_log函数。
 */
int32_t udebug(const char * format, ...)
{
	va_list ap;
	static char buff[1024];  //这里未检查溢出 :(
	int32_t len;
	
	va_start(ap, format);
	if((len = vsprintf(buff, format, ap)) == -1 )
	{
		debug("udebug: parse format error\n");
		va_end(ap);
		return -1;
	}
	else
	{
		debug(buff);
		va_end(ap);
		return len;
	}
}

/*
 * 格式化输出到标准输出端
 * 使用方法类似于iprint_log函数。
 */
int32_t printf(const char * format, ...)
{
	va_list ap;
	static char buff[1024];  //这里未检查溢出 :(
	static char * err_msg = "printf: parse error\n";
	int32_t len;
	
	va_start(ap, format);
	if((len = vsprintf(buff, format, ap)) == -1)
	{
		write(FD_STDERR, err_msg, strlen(err_msg));
		va_end(ap);
		return -1;
	}
	else
	{
		write(FD_STDOUT, buff, (uint32_t)len);
		va_end(ap);
		return len;
	}
}

/*
 * 从指定文件中读取一行数据到缓冲区中
 * fd: 已打开的文件描述符，允许读取；
 * buf: 缓冲区起始地址；
 * n: 最多读取的字符数为n-1；
 *
 * 从fd指定文件中读取最多n-1个字符，在遇到'\n'字符或者文件结尾时结束读取；
 * 如果读到了'\n'字符则也将保存该字符到buf中；
 * 读取结束后在最后一个被读取字符的后面添加一个NUL字符。
 * 成功返回实际读取字符个数，失败返回-1；
 */
int32_t gets(int32_t fd, void * buf, int32_t n)
{
	int32_t old_n;
	char ch;
	int32_t ret;

	old_n = n;
	while(n > 1)
	{
		if((ret = read(fd, &ch, 1)) == -1)
			return -1;
		else if(ret == 0)
			break;
		*((char *)buf) = ch;
		n--;
		buf++;
		if(ch == '\n')
			break;
	}
	*((char *)buf) = '\0';

	return old_n - n;
}

/*
 * 从指定文件中读取一个字符
 * fd: 已打开的文件描述符，允许读取；
 *
 * 从fd指定文件中读取一个字符，转换为int32_t类型并返回；失败或者遇到文件结尾则返回-1；
 */
int32_t getc(int32_t fd)
{
	char ch;
	
	if(read(fd, &ch, 1) <= 0)
		return -1;
	return (int32_t)ch;
}
