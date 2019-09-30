#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "process.h"
#include "syscall.h"

/*
 * 创建子进程
 */
int32_t sys_fork(void)
{
	return fork();
}

/*
 * 结束当前进程
 */
int32_t sys_exit(void)
{
	int32_t retval;
	if(get_int_arg(0, (uint32_t *)&retval) != 0)
		PANIC("sys_exit: illegal argument");
	
	exit(retval);
}

/*
 * 等待子进程退出
 */
int32_t sys_wait(void)
{
	int32_t * retval;
	if(get_ptr_arg(0, (uint32_t *)&retval, sizeof(*retval)) != 0)
		PANIC("sys_wait: illegal argument");

	return wait(retval);
}

/*
 * 返回当前进程pid
 */
int32_t sys_getpid(void)
{
	return (int32_t)(cpu.cur_proc->pid);
}
