/*
 * 便于实现系统调用的框架函数
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "syscall.h"
#include "process.h"
#include "parameters.h"

/* 已实现的系统调用 */
extern int32_t sys_debug(void);
extern int32_t sys_exec(void);
extern int32_t sys_fork(void);
extern int32_t sys_exit(void);
extern int32_t sys_wait(void);
extern int32_t sys_getpid(void);
extern int32_t sys_dup(void);
extern int32_t sys_read(void);
extern int32_t sys_write(void);
extern int32_t sys_open(void);
extern int32_t sys_close(void);
extern int32_t sys_fstat(void);
extern int32_t sys_link(void);
extern int32_t sys_unlink(void);
extern int32_t sys_mkdir(void);
extern int32_t sys_mknod(void);
extern int32_t sys_chdir(void);
extern int32_t sys_pipe(void);

/* 系统调用指针表 */
static int32_t (* syscall_table[])(void) = {
	[SYS_NUM_debug]		= sys_debug,
	[SYS_NUM_exec]		= sys_exec,
	[SYS_NUM_fork]		= sys_fork,
	[SYS_NUM_exit]		= sys_exit,
	[SYS_NUM_wait]		= sys_wait,
	[SYS_NUM_getpid]	= sys_getpid,
	[SYS_NUM_dup]		= sys_dup,
	[SYS_NUM_read]		= sys_read,
	[SYS_NUM_write]		= sys_write,
	[SYS_NUM_open]		= sys_open,
	[SYS_NUM_close]		= sys_close,
	[SYS_NUM_fstat]		= sys_fstat,
	[SYS_NUM_link]		= sys_link,
	[SYS_NUM_unlink]	= sys_unlink,
	[SYS_NUM_mkdir]		= sys_mkdir,
	[SYS_NUM_mknod]		= sys_mknod,
	[SYS_NUM_chdir]		= sys_chdir,
	[SYS_NUM_pipe]		= sys_pipe
};

static char * syscall_str_table[] = {
	[SYS_NUM_debug]		= "debug",
	[SYS_NUM_exec]		= "exec",
	[SYS_NUM_fork]		= "fork",
	[SYS_NUM_exit]		= "exit",
	[SYS_NUM_wait]		= "wait",
	[SYS_NUM_getpid]	= "getpid",
	[SYS_NUM_dup]		= "dup",
	[SYS_NUM_read]		= "read",
	[SYS_NUM_write]		= "write",
	[SYS_NUM_open]		= "open",
	[SYS_NUM_close]		= "close",
	[SYS_NUM_fstat]		= "fstat",
	[SYS_NUM_link]		= "link",
	[SYS_NUM_unlink]	= "unlink",
	[SYS_NUM_mkdir]		= "mkdir",
	[SYS_NUM_mknod]		= "mknod",
	[SYS_NUM_chdir]		= "chdir",
	[SYS_NUM_pipe]		= "pipe"
};

/*
 * 根据当前进程指定的系统调用号，执行对应的系统调用函数。
 */
void syscall(void)
{
	uint32_t num;

	num = cpu.cur_proc->tf->eax;

	/* DEBUG */
	iprint_log("syscall: %s[%u] has raised a syscall `%s'\n", 
			cpu.cur_proc->name,
			cpu.cur_proc->pid,
			syscall_str_table[num]);
	/* 针对debug系统调用特殊处理 */
	if(num == 0)
	{
		cpu.cur_proc->tf->eax = syscall_table[num]();
		return;
	}


	if(num > 0 && 
			num < sizeof(syscall_table)/sizeof(syscall_table[0]) &&
			syscall_table[num] != NULL)
		cpu.cur_proc->tf->eax = syscall_table[num]();
	else
		PANIC("syscall: invalid syscall num");
}

/*
 * 检查当前进程用户地址空间地址addr是否在非用户栈的有效地址空间中，
 * 即是否可以通过该地址访问至少一个字节的数据。
 * addr： 用户地址空间中的地址；
 * 成功返回1，失败返回0；
 */
int32_t within_nstack(uint32_t addr)
{
	if(addr >= PROC_LOAD_ADDR && addr < PROC_LOAD_ADDR + cpu.cur_proc->size)
		return 1;
	else return 0;
}

/*
 * 检查当前进程用户地址空间地址addr是否在用户栈中，
 * 即是否可以通过该地址访问至少一个字节的数据。
 * addr： 用户地址空间中的地址；
 * 成功返回1，失败返回0；
 */
int32_t within_stack(uint32_t addr)
{
	if(addr >= PROC_USER_STACK_ADDR && addr < PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE)
		return 1;
	else return 0;
}

/*
 * 从当前进程用户地址空间addr处取出一个4字节大小的参数到ap指定位置处。
 * addr： 用户地址空间中的地址；
 * ap： 指向保存参数的位置；
 * 成功返回0且结果将写入*ap中，失败返回-1且*ap中的值不会被修改；
 */
int32_t fetch_int(uint32_t addr, uint32_t * ap)
{
	if((within_stack(addr) && within_stack(addr + 3)) ||
			(within_nstack(addr) && within_nstack(addr + 3)))
	{
		*ap = *((uint32_t *)addr);
		return 0;
	}
	return -1;
}

/*
 * 检查当前进程用户地址空间addr处开始是否为一个NUL结尾的字符串。
 * addr： 用户地址空间中的地址；
 * 是则返回其长度，否则返回-1；
 */
int32_t check_str(uint32_t addr)
{
	char * s;

	s = (char *)addr;
	if(within_nstack(addr))
	{
		do
		{
			if(*s == '\0')
				return (int32_t)(s - (char *)addr);
			s++;
		}while(within_nstack((uint32_t)s));
	}
	else if(within_stack(addr))
	{
		do
		{
			if(*s == '\0')
				return (int32_t)(s - (char *)addr);
			s++;
		}while(within_stack((uint32_t)s));
	}

	return -1;
}

/*
 * 从当前进程用户栈中取出第n个4字节大小的参数写入到ap所指向的位置，遵循cdecl。
 * n： 从左往右第n个4字节大小的参数，n从0开始；
 * ap： 指向保存参数的位置；
 * 成功返回0且参数将写入ap所指向的位置，失败返回-1且*ap中的值不会被修改；
 */
int32_t get_int_arg(uint32_t n, uint32_t * ap)
{
	return fetch_int(cpu.cur_proc->tf->esp + 4 + 4 * n, ap);
}

/*
 * 从当前进程用户栈取出第n个4字节大小的参数作为指针，
 * 写入到pp所指向的位置，并检查该指针指向的对象是否在进程的有效用户地址空间中，
 * 遵循cdecl。
 * n： 从左往右第n个4字节大小的参数，n从0开始；
 * pp： 指向保存指针的位置；
 * size： 该指针指向的对象大小；
 * 成功返回0且参数将写入pp所指向的位置，失败返回-1且*pp中的值不会被修改；
 */
int32_t get_ptr_arg(uint32_t n, uint32_t * pp, uint32_t size)
{
	uint32_t tmp;
	
	if((get_int_arg(n, &tmp) == -1) || (tmp + size < tmp))
		return -1;
	if((within_stack(tmp) && within_stack(tmp + size)) ||
			(within_nstack(tmp) && within_nstack(tmp + size)))
	{
		*pp = tmp;
		return 0;
	}

	return -1;
}

/*
 * 从当前进程用户栈中取出第n个4字节大小的参数作为字符串指针，
 * 写入pp所指向的位置，并检查该指针指向的对象是否为NUL结尾的字符串，遵循cdecl。
 * n： 从左往右第n个4字节大小的参数，n从0开始；
 * pp： 指向保存指针的位置；
 *
 * 如果参数取出成功且所指向对象确实是NUL结尾的字符串，
 * 则返回字符串长度且字符串指针将写入pp所指向的位置，
 * 否则返回-1且*pp中的值不会被修改；
 */
int32_t get_str_arg(uint32_t n, uint32_t * pp)
{
	uint32_t tmp;
	int32_t ret;

	if(get_int_arg(n, &tmp) == -1)
		return -1;
	if((ret = check_str(tmp)) == -1)
		return -1;
	*pp = tmp;
	return ret;
}

/*
 * DEBUG
 */
#include "vm_tools.h"
#include "vmm.h"
#include "fs.h"
#include "inode.h"
#include "path.h"
int32_t sys_debug(void)
{
	char * str;
	
	if(get_str_arg(0, (uint32_t *)&str) == -1)
		PANIC("sys_debug: invalid str argument");

	iprint_log("%s", str);
	//printk("%s", str);


	return 5;
}

