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
 * 移除链接
 * cmd path ...
 * 移除指定链接，如果path指定的是目录且目录不为空，
 * 则不会移除该链接。如果某个path无法移除则会跳过该path，
 * 继续后续的处理。
 * 成功全部移除返回0，否则返回-1。
 */
int32_t main(int32_t argc, char * argv[])
{
	int32_t error;

	error = 0;
	if(argc <= 1)
	{
		printf("unlink: usage: unlink path ...\n");
		return -1;
	}
	for(int32_t i = 1; i < argc; i++)
	{
		if(unlink(argv[i]) < 0)
		{
			error = 1;
			printf("unlink: remove link `%s' failed\n", argv[i]);
		}
	}
	if(error)
		return -1;
	else return 0;
}
