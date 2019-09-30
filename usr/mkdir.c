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
 * cmd path ...
 * 创建一个或多个目录，如果其中某个目录创建失败（已经存在或
 * 其他原因）则跳过该目录的创建，继续处理后续需要创建的目录。
 *
 * 全部创建成功返回0，否则返回-1。
 */
int32_t main(int32_t argc, char * argv[])
{
	int32_t is_error;
	
	is_error = 0;
	if(argc <= 1)
	{
		printf("mkdir: no path specified\n");
		return -1;
	}

	for(int32_t i = 1; i < argc; i++)
	{
		if(mkdir(argv[i]) < 0)
		{
			printf("mkdir: create `%s' failed\n", argv[i]);
			is_error = 1;
		}
	}

	if(is_error)
		return -1;
	else return 0;
}
