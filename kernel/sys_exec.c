#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "syscall.h"
#include "parameters.h"

/*
 * 检查用户地址空间传递的参数，并调用函数加载指定程序以替换当前进程的内存镜像；
 * 成功则从所加载程序入口点开始执行，失败则返回-1；
 */
extern int32_t exec(const char * path, char * argv[]);
int32_t sys_exec(void)
{
	char * path;
	uint32_t argv;
	uint32_t str_arg;
	
	/* path是从左往右第0个参数 */
	if(get_int_arg(0, (uint32_t *)&path) == -1)
		return -1;
	/* argv是从左往右第1个参数 */
	if(get_int_arg(1, &argv) == -1)
		return -1;
	
	/* 检查path */
	if(check_str((uint32_t)path) <= 0)
		return -1;
	
	/* 检查argv数组 */
	for(int32_t i = 0; i < MAX_ARG_NUM; i++)
	{
		if(fetch_int(argv + i * 4, &str_arg) == -1) //取出argv[i]
			return -1;
		if(str_arg == (uint32_t)NULL) //argv[i]为NULL，没有更多字符串参数了
			break;
		if(check_str(str_arg) == -1) //argv[i]不是一个合法的字符串参数
			return -1;
	}

	/* DEBUG */
	iprint_log("sys_exec: arguments check success\n");
	iprint_log("sys_exec: path: %s\n", path);
	char ** _argv = (char **)argv;
	for(int32_t i = 0; i < MAX_ARG_NUM; i++)
	{
		if(_argv[i] == NULL)
			break;
		iprint_log("sys_exec: argv[%d]: %s\n", i, _argv[i]);
	}
	
	/* 检查通过 */
	return exec(path, (char **)argv);
}
