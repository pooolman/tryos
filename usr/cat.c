#include <stdint.h>
#include <stddef.h>

#include "sys.h"
#include "ulib.h"
#include "fcntl.h"
#include "string.h"
#include "stat.h"
#include "fs.h"
#include "parameters.h"

/*
 * 从stdin循环读取数据，写入stdout，直至所有数据读取完毕或者出错
 * 如果读取/写入出错，返回-1，否则返回0；
 */
static int32_t rw_file(void)
{
	static uint8_t buf[128];
	int32_t size;
	
	while((size = read(FD_STDIN, buf, sizeof(buf))) > 0)
	{
		if(write(FD_STDOUT, buf, (uint32_t)size) < 0)
			return -1;
	}
	if(size < 0)
		return -1;
	else return 0;
}

/*
 * 从标准输入读取内容，并向标准输出上输出:
 * 	cmd [path] ...
 * 当没有指定path时，从标准输入循环读取数据，并向标准输出上输出，
 * 直到标准输入中没有数据可读（需要ctrl+d的支持，目前未实现）；
 * 当指定一个或多个path时，则按照path出现的顺序，
 * 依次向标准输出输出它们的所有内容；只能读取普通文件。
 * 							
 * 成功执行则以0退出，否则以-1退出；
 */
int32_t main(int argc, char * argv[])
{
	int32_t i;
	int32_t fd;
	stat_t st;

	if(argc == 1)
	{
		/*
		if(rw_file() == -1)
		{
			//没有指定path
			printf("cat: read or write file failed\n");
			return -1;
		}
		else return 0;
		*/
		/* 由于在读取pipe时，可能返回-1但不表
		 * 示出错，没法和普通文件读取出错区分
		 * 所以不检查其返回值
		 */
		rw_file();
	}
	else
	{
		for(i = 1; i < argc; i++)
		{
			close(FD_STDIN);
			if((fd = open(argv[i], O_RDONLY)) < 0)
			{
				printf("cat: open `%s' failed\n", argv[i]);
				return -1;
			}
			if(fstat(fd, &st) < 0)
			{
				printf("cat: stat `%s' failed\n", argv[i]);
				return -1;
			}
			if(st.type != FILE_INODE)
			{
				printf("cat: `%s' is not a regular file\n", argv[i]);
				return -1;
			}
			/*
			if(rw_file() == -1)
			{
				printf("cat: read or write file failed\n");
				return -1;
			}
			*/
			/* 由于在读取pipe时，可能返回-1但不表
			 * 示出错，没法和普通文件读取出错区分
			 * 所以不检查其返回值
			 */
			rw_file();
		}
		return 0;
	}
	return 0;
}
