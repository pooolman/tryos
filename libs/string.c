/*
 * 本文件提供一些与字符串处理有关的函数实现
 */

#include "string.h"


/* 这些常量用于 itos 函数的flags参数，第1/2/3项之间互斥*/
#define ONE_BYTE_SIZE 1		//1 Byte size
#define TWO_BYTES_SIZE 2	//2 Bytes size
#define FOUR_BYTES_SIZE 3	//4 Bytes size
#define SIZE_MASK 3		//use to clasify ONE_BYTE_SIZE/TWO_BYTES_SIZE/FOUR_BYTES_SIZE
#define IS_SIGNED 4		//is a signed value? default is unsigned value
#define LOWER 8			//use lower case? default is upper case

/* 该宏函数用于获取n%base的值，调用该函数后n的值为n/base的商 */
#define DO_DIV(n,base) ({ \
				uint32_t __res; \
				__asm__("divl %4":"=a" (n),"=d" (__res):"0" (n),"1" (0),"r" (base)); \
				__res; })

/*
#include <stdio.h>
void debug_itos(int value, int base, int flags )
{
	printf("value: %d, base: %d, flags: ", value, base);
	switch(flags & SIZE_MASK)
	{
		case ONE_BYTE_SIZE: 
			printf("ONE_BYTE_SIZE ");
			break;
		case TWO_BYTES_SIZE:
			printf("TWO_BYTES_SIZE ");
			break;
		case FOUR_BYTES_SIZE:
			printf("FOUR_BYTES_SIZE ");
			break;
	}
	if(flags & IS_SIGNED)
		printf("IS_SIGNED ");
	else printf("IS_UNSIGNED ");
	if(flags & LOWER)
		printf("LOWER\n");
	else printf("UPPER\n");
}
*/

/*
 * 用于将一个整数值转换为对应格式的字符串，可以指定该值是否按照有符号数解释，以及使用何种进制
 * 来转换为字符串。负数则会加上一个'-'前缀，正数没有'+'前缀。
 * str: 指向保存字符串的缓存区(这个缓存区要足够容纳整个字符串，包括结尾的'\0'字符)
 * value: 待转换的数值
 * base: 使用的进制，base取值范围为2-36
 * flags: 指定value参数中有效字节数（即只使用其低端多少个字节，该属性必须被指定）以及value是否有符号
 * 
 * 返回指向字符串结尾（指向'\0'字符）的指针
 *
 * 注意：如果使用了不符合约定的参数（例如，value中存储的数值不能使用flags中指定的有效字节数存储），那么结果是未定义的
 */

static char * itos(char * str, int value, int base, int flags)
{

	//debug:
	//debug_itos(value, base, flags);


	const char * char_table = NULL; //指向一个字符映射表
	static char tmp[32]; //存储转换后的不加前缀的字符序列，最长也只需要32个字节
	int len = 0; //tmp数组中字符序列的长度
	const char * sp = NULL; //指向"0"或者"0x"/"0X"前缀

	if(flags & LOWER)
		char_table = "0123456789abcdefghijklmnopqrstuvwxyz";
	else
		char_table = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	switch(flags & SIZE_MASK)
	{
		case ONE_BYTE_SIZE:
			if(flags & IS_SIGNED)
			{
				int8_t value_ = (int8_t)value;
				if(value_ < 0)
				{
					value_ = -value_;  //如果value_为最小的负数（-128），那么value_无法保存其绝对值，出现的结果还不能被解释。下同
					*str++ = '-'; //负数则加上一个'-'前缀,下同
				}
				do{
					tmp[len++] = char_table[DO_DIV(value_, base)];
				}while(value_);
			}else
			{
				uint8_t value_ = (uint8_t)value;
				do{
					tmp[len++] = char_table[DO_DIV(value_, base)];
				}while(value_);
			}
			break;
		case TWO_BYTES_SIZE:
			if(flags & IS_SIGNED)
			{
				int16_t value_ = (int16_t)value;
				if(value_ < 0)
				{
					value_ = -value_;
					*str++ = '-';
				}
				do{
					tmp[len++] = char_table[DO_DIV(value_, base)];
				}while(value_);
			}else
			{
				uint16_t value_ = (uint16_t)value;
				do{
					tmp[len++] = char_table[DO_DIV(value_, base)];
				}while(value_);
			}
			break;
		case FOUR_BYTES_SIZE:
			if(flags & IS_SIGNED)
			{
				int32_t value_ = (int32_t)value;
				if(value_ < 0)
				{
					value_ = -value_;
					*str++ = '-';
				}
				do{
					tmp[len++] = char_table[DO_DIV(value_, base)];
				}while(value_);
			}else
			{
				uint32_t value_ = (uint32_t)value;
				do{
					tmp[len++] = char_table[DO_DIV(value_, base)];
				}while(value_);
			}
			break;
	}
	
	/*设置"0"/"0x"/"0X"前缀 */
	if(base == 16)
	{
		if(flags & LOWER)
			sp = "0x";
		else
			sp = "0X";
	}else if(base == 8)
		sp = "0";
	else
		sp = NULL;

	/* 构造整个字符串 */
	if(sp)
		while(*sp)
			*str++ = *sp++;
	while(len-- > 0)
		*str++ = tmp[len];
	*str = '\0';
	
	/*返回指向字符串尾部的指针 */
	return str;
}




/*
 * 用于向缓存区输出格式化字符串。使用方法类似于stdio中的vsprintf函数。
 * 函数将返回所写入的总字符数（不包括结尾的'\0'字符），解析失败返回-1；
 * 详细的说明参考demand_skeleton_analysis.txt文件
 */
int vsprintf(char * buff, const char * format, va_list ap)
{
	char * bp = buff; //一个指向buff的移动指针
	int flags = 0;

	/* 循环处理，直到遇到一个'\0'字符*/
	for(; *format != '\0'; format++)
	{
		/* 普通字符直接复制过去 */
		if(*format != '%')
		{
			*bp++ = *format;
			continue;
		}
		
		/* 遇到一个 conversion specification 的开始字符 */
		
		/* 获取length modifier */
		format++; //跳过'%'字符
		flags = 0; //每一轮都要清空flags
		switch(*format)
		{
			case 'h':
				if(*(format+1) == 'h')
				{
					format++;
					flags |= ONE_BYTE_SIZE;
				}else
					flags |= TWO_BYTES_SIZE;
				format++;
				break;
			case 'l':
			case 'z':
				flags |= FOUR_BYTES_SIZE;
				format++;
				break;
		}
		
		/* 根据conversin specifier 进行处理 */
		switch(*format)
		{
			case 'c':
				if((flags & SIZE_MASK) != 0) //c前面不能使用length modifier
					return -1;
				*bp++ = (unsigned char)va_arg(ap, int); //按照int类型长度取值，转换为unsigned char，应该也可以
				break;
			case 's':
				if((flags & SIZE_MASK) != 0) //s前面不能使用length modifier
					return -1;
				char * str = va_arg(ap, char *);
				while(*str != '\0')
					*bp++ = *str++;
				break;
			case 'p':
				if((flags & SIZE_MASK) != 0) //p前面不能使用length modifier
					return -1;
				flags |= FOUR_BYTES_SIZE; //强制使用四字节长度
				bp = itos(bp, (int)va_arg(ap, void *), 16, flags);
				break;
			case '%':
				if((flags & SIZE_MASK) != 0) //%前面不能使用length modifier
					return -1;
				*bp++ = '%';
				break;
			case 'd':
				if((flags & SIZE_MASK) == 0) //没有指定length modifier，则默认为四字节长度，下同
					flags |= FOUR_BYTES_SIZE;
				flags |= IS_SIGNED; //因为默认为unsigned，所以这里需要重新设置
				bp = itos(bp, va_arg(ap, int), 10, flags);
				break;
			case 'o':
				if((flags & SIZE_MASK) == 0)
					flags |= FOUR_BYTES_SIZE;
				bp = itos(bp, va_arg(ap, int), 8, flags);
				break;
			case 'u':
				if((flags & SIZE_MASK) == 0)
					flags |= FOUR_BYTES_SIZE;
				bp = itos(bp, va_arg(ap, int), 10, flags);
				break;
			case 'x':
				if((flags & SIZE_MASK) == 0)
					flags |= FOUR_BYTES_SIZE;
				flags |= LOWER; //因为默认是upper case，所以这里需要重新设置
				bp = itos(bp, va_arg(ap, int), 16, flags);
				break;
			case 'X':
				if((flags & SIZE_MASK) == 0)
					flags |= FOUR_BYTES_SIZE;
				bp = itos(bp, va_arg(ap, int), 16, flags);
				break;
			case 'b':
				if((flags & SIZE_MASK) == 0)
					flags |= FOUR_BYTES_SIZE;
				bp = itos(bp, va_arg(ap, int), 2, flags);
				break;
			default:
				return -1; //对于任何不合法的conversion specifier，函数都出错，返回-1
		}
	}
	*bp = '\0';
	return (int)(bp - buff);
}


/*
 * 向buffer指向的缓存区中写入count个字节的ch值（写入前被转换成char类型）,并返回buffer指针
 */
void * memset(void * buffer, int ch, size_t count)
{
	if(buffer)
	{
		char * buf = (char *)buffer;
		for(size_t i = 0; i < count; i++)
			buf[i] = (char)ch;
		return (void *)buf;
	}
	else return NULL;
}

/*
 * 从src指向的缓存区中复制count个字节到dst所指向的缓存区，src和dst所指向的缓存区可以重叠。
 * 返回dst指针。
 */
void * memmove(void * dst, const void * src, size_t count)
{
	const char * s = src;
	char * d = dst;
	
	if(s == d)
		return dst;
	
	if(s < d && s + count > d)
	{
		s = s + count - 1;
		d = d + count - 1;
		while(count-- > 0)
			*d-- = *s--;
	}
	else
		while(count-- > 0)
			*d++ = *s++;

	return dst;
}

/*
 * 从src指向的缓存区中复制count个字节到dst所指向的缓存区，src和dst所指向的缓存区可以重叠
 * 返回dst指针
 */
void * memcpy(void * dst, const void * src, size_t count)
{
	return memmove(dst, src, count);
}

/*
 * 返回字符串str的长度，不包括结尾的NUL字符。
 */
size_t strlen(const char * str)
{
	size_t len = 0;
	while(*str++)
		len++;
	
	return len;
}

/*
 * 从src指向的字符串中复制count个字节到dst中，如果count超过src指向字符串的长度（不包括结尾的NUL字符），
 * 则只复制src指向字符串中的所有字符；不管何种情况都在dst结尾处添加一个NUL字符。
 * 返回dst指针。
 */
char * strncpy(char * dst, const char * src, size_t count)
{
	size_t actual_count;
	
	if(count <= 0)
	{
		*dst = 0;
		return dst;
	}
	
	if(count <= strlen(src))
		actual_count = count;
	else
		actual_count = strlen(src);
	
	memcpy(dst, src, actual_count);
	*(dst + actual_count) = 0;

	return dst;
}

/*
 * 将src指向的字符串复制到dst所指向的位置，包括结尾的NUL字符，最多复制count个字节(包括src结尾的NUL字符)。
 * 注意：如果src的前count个字节中没有NUL字符，则dst尾部不会以NUL字符结尾。
 * 返回dst指针。
 */
char * sstrncpy(char * dst, const char * src, size_t count)
{
	if(count <= 0)
		return dst;

	size_t src_len = strlen(src);
	if(src_len < count)
	{
		memcpy(dst, src, src_len);
		*(dst + src_len) = '\0';
	}
	else
		memcpy(dst, src, count);

	return dst;
}

/*
 * 比较s1和s2两个字符串，比较长度不超过n个字符，s1和s2应该以NUL字符结尾。
 * 在比较过程中如果发现s1中某个字符大于/小于s2中对应字符，则相应返回大于0/小于0的值，如果全部相同则返回0。
 */
int32_t strncmp(const char * s1, const char * s2, size_t n)
{
	for(; *s1 != '\0' && *s2 != '\0' && n > 0; n--, s1++, s2++)
	{
		if(*s1 == *s2)
			continue;
		return (int32_t)(*s1 - *s2);
	}
	if(n == 0)
		return 0;
	return (int32_t)(*s1 - *s2);
}

/*
 * 将src中最多n个字节附加到dst的结尾处，覆盖其最后的'\0'字符，
 * 随后在整个字符串的结尾加上'\0'；
 * 如果src中的字符个数大于或等于n，则它不需要一个'\0'字符结尾。
 * 要保证dst能够容纳strlen(dst)+n+1个字符。
 */
char * strncat(char * dst, const char * src, size_t n)
{
	size_t i;
	size_t dst_len = strlen(dst);

	for(i = 0; i < n && src[i] != '\0'; i++)
		dst[dst_len + i] = src[i];
	dst[dst_len + i] = '\0';

	return dst;
}

/*
 * 返回字符c在字符串s中第一次出现的位置；
 * 没有找到则返回NULL
 */
char * strchr(const char * s, char c)
{
	while(*s && *s != c)
		s++;
	if(!(*s))
		return NULL;
	else return (char *)s;
}

/*
void debug_vsprintf(const char * format, ...)
{
	va_list ap;
	char buff[1024];
	
	va_start(ap, format);
	int ret = vsprintf(buff, format, ap);
	va_end(ap);

	if(ret == -1)
		printf("An error ocurred\n");
	else printf("buff: |%s|\nlen: %d\n", buff, ret);
}


#include <stdio.h>

char buf[] = "abcdefghijk";
int main(void)
{
	char * s = &buf[4];
	char * d = &buf[0];
	size_t count = 5;

	printf("buf: %p content: %s\n", buf, buf);
	char * res = strncpy(d, s, count);
	printf("count: %d\n", (int)count);

	printf("buf: %p content: %s\n", buf, buf);
	printf("d: %p content: %s\n", d, d);
	printf("s: %p content: %s\n", s, s);


	if(d == res )
		printf("*s: %c *d: %c count: %u buf: %s\n", *s, *d, (unsigned int)count, buf);

	return 0;
}
*/
