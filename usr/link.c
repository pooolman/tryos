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
 * cmd path1 path2
 * 添加一个名为path2的链接指向path1所指向的文件。
 * 成功返回0，失败（path1不存在、path2已存在等）返回-1。
 */
int32_t main(int32_t argc, char * argv[])
{
	if(argc != 3)
	{
		printf("link: usage: link newpath oldpath\n");
		return -1;
	}
	return link(argv[1], argv[2]);
}
