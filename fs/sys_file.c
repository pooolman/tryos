/*
 * 文件系统相关的系统调用
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "syscall.h"
#include "file.h"
#include "stat.h"
#include "fs.h"
#include "inode.h"
#include "path.h"
#include "process.h"
#include "pipe.h"

extern int32_t do_open(const char * path, uint32_t mode);
extern int32_t do_link(const char * oldpath, const char * newpath);
extern int32_t do_unlink(const char * path);
extern mem_inode_t * create(const char * path, uint16_t type, uint16_t major, uint16_t minor);

/*
 * 获取并检查用户模式传递给内核的文件描述符
 * n：第n个4字节参数；
 * pfd：保存文件描述符的变量；
 * pfp：保存打开文件结构指针的变量；
 *
 * 成功返回0，失败返回-1且不改变*pfd和*pfp的值；
 */
static int32_t get_fd_arg(uint32_t n, int32_t * pfd, file_t ** pfp)
{
	int32_t fd;
	file_t * fp;
	
	if(get_int_arg(n, (uint32_t *)&fd) == -1)
		return -1;
	if(fd < 0 || fd >= PROC_OPEN_FD_NUM || (fp = cpu.cur_proc->open_files[fd]) == NULL)
		return -1;
	
	if(pfd)
		*pfd = fd;
	if(pfp)
		*pfp = fp;
	return 0;
}

/*
 * 复制一个文件描述符
 * 用户模式参数：
 * 	fd: 待复制的文件描述符； 
 * 用户模式返回值：
 * 	成功返回一个最小、未被使用的文件描述符副本，失败返回-1；
 */
int32_t sys_dup(void)
{
	file_t * fp;

	if(get_fd_arg(0, NULL, &fp) == -1)
		return -1;
	return alloc_fd(dup_file(fp));
}

/*
 * 从指定文件当前偏移量处读取n个字节到buf中
 * 从当前文件偏移量处读取n个字节数据到buf中，文件偏移量将按照
 * 实际读取的字节数偏移，实际读取的字节数可能少于n；
 * 如果该文件为设备文件或者管道，则其行为取决于设备或管道。
 * 用户模式参数：
 * 	fd: 待读取的文件；
 * 	buf: 目标缓冲区起始地址；
 * 	n: 要求读取的字节数；
 * 用户模式返回值：
 * 	成功返回实际读取的字节数，失败返回-1且偏移量不被改变；
 */
int32_t sys_read(void)
{
	file_t * fp;
	void * buf;
	uint32_t n;

	if(get_fd_arg(0, NULL, &fp) == -1)
		return -1;
	if(get_int_arg(2, &n) == -1)
		return -1;
	if(get_ptr_arg(1, (uint32_t *)&buf, n) == -1)
		return -1;

	return read_file(fp, buf, n);
}

/*
 * 向指定文件当前偏移量处写入从buf开始的n个字节
 * 向指定文件当前偏移量处写入从buf开始的n个字节，文件偏移量将
 * 偏移n个字节；
 * 如果该文件为设备文件或管道，则其行为取决于设备或管道。
 * 用户模式参数：
 * 	fd: 待写入的文件；
 * 	buf: 源缓冲区起始地址；
 * 	n: 要求写入的字节数；
 * 用户模式返回值：
 * 	成功返回n，失败返回-1且偏移量不被改变；
 */
int32_t sys_write(void)
{
	file_t * fp;
	void * buf;
	uint32_t n;

	if(get_fd_arg(0, NULL, &fp) == -1)
		return -1;
	if(get_int_arg(2, &n) == -1)
		return -1;
	if(get_ptr_arg(1, (uint32_t *)&buf, n) == -1)
		return -1;

	return write_file(fp, buf, n);
}

/*
 * 打开或创建路径所指定的文件并返回最小可用文件描述符
 * 使指定文件与一个新的打开文件结构关联，可能会创建这个文件，
 * 返回与这个打开文件结构关联的文件描述符；
 * 不能用于创建设备文件/目录文件/管道；
 * 用户模式参数：
 * 	path: 指定文件的路径；
 * 	flags: 标志位，参考文件系统接口支持函数部分；
 * 用户模式返回值：
 * 	成功返回最小可用文件描述符，失败返回-1；
 */
int32_t sys_open(void)
{
	char * path;
	uint32_t flags;

	if(get_str_arg(0, (uint32_t *)&path) <= 0)
		return -1;
	if(get_int_arg(1, &flags) == -1)
		return -1;
	
	return do_open(path, flags);
}

/*
 * 关闭文件描述符
 * 关闭所指定的文件描述符使其可重新使用；如果该文件描述符是最
 * 后一个对对应打开文件结构的引用则该打开文件结构将被释放掉；
 * 如果该打开文件结构是最后一个对对应inode文件的引用且该inode
 * 文件已经被移除了所有路径链接则该文件将被删除；
 * 如果该打开文件结构是最后一个被关闭的管道端口则管道将被释放。
 * 用户模式参数：
 * 	fd: 待关闭的文件描述符；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1；
 */
int32_t sys_close(void)
{
	file_t * fp;
	int32_t fd;

	if(get_fd_arg(0, &fd, &fp) == -1)
		return -1;
	close_file(fp);
	cpu.cur_proc->open_files[fd] = NULL;
	return 0;
}

/*
 * 获取文件描述符对应inode的信息
 * 如果文件描述符对应设备文件，则其含义取决于设备；
 * 如果对应管道，则返回-1；
 * 用户模式参数：
 * 	fd: 目标文件描述符；
 * 	st: stat结构指针；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1；
 */
int32_t sys_fstat(void)
{
	file_t * fp;
	stat_t * st;

	if(get_fd_arg(0, NULL, &fp) == -1)
		return -1;
	if(get_ptr_arg(1, (uint32_t *)&st, sizeof(*st)) == -1)
		return -1;
	return stat_file(fp, st);
}

/*
 * 为一个文件创建一个新路径
 * 为一个已存在的文件创建一条新的路径，如果新路径已经存在则不
 * 会被覆盖；具体参考文件系统接口支持函数；
 * 用户模式参数：
 * 	oldpath: 指定文件；
 * 	newpath: 一条新的指向指定文件的路径；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1：
 */
int32_t sys_link(void)
{
	char * oldpath;
	char * newpath;

	if(get_str_arg(0, (uint32_t *)&oldpath) <= 0)
		return -1;
	if(get_str_arg(1, (uint32_t *)&newpath) <= 0)
		return -1;
	
	return do_link(oldpath, newpath);
}

/*
 * 删除一条路径，可能会删除该路径指定的文件
 * 从文件系统中删除一条路径。如果这条路径是最后一条引用该文件
 * 的路径且没有进程打开该文件则该文件将被删除，所占据空间将被
 * 释放掉。如果这条路径是最后一条引用该文件的路径但是仍有进程
 * 打开该文件则该文件将被保留直到最后一个对该文件对应inode的引
 * 用被丢弃。
 * 用户模式参数：
 * 	path: 指定删除的路径；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1；
 */
int32_t sys_unlink(void)
{
	char * path;
	
	if(get_str_arg(0, (uint32_t *)&path) <= 0)
		return -1;
	return do_unlink(path);
}

/*
 * 创建一个目录文件
 * 用户模式参数：
 * 	path: 该目录的路径；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1；
 */
int32_t sys_mkdir(void)
{
	char * path;

	if(get_str_arg(0, (uint32_t *)&path) <= 0)
		return -1;
	if(create(path, DIR_INODE, 0, 0) == NULL)
		return -1;
	return 0;
}

/*
 * 创建一个文件系统节点
 * 在文件系统中创建一个结点，当结点类型为设备时，
 * 通过major/minor指定其设备号，否则major/minor将被忽略。
 * 用户模式参数：
 * 	path: 该节点的路径；
 * 	type: 节点类型，目前可用类型为字符设备；
 * 	major: 主设备号；
 * 	minor: 从设备号；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1；
 */
int32_t sys_mknod(void)
{
	char * path;
	uint32_t type;
	uint32_t major;
	uint32_t minor;

	if(get_str_arg(0, (uint32_t *)&path) <= 0)
		return -1;
	if(get_int_arg(1, &type) == -1)
		return -1;
	if(get_int_arg(2, &major) == -1)
		return -1;
	if(get_int_arg(3, &minor) == -1)
		return -1;

	if(type != CHR_DEV_INODE)
		PANIC("sys_mknod: unsupported node type");

	if(create(path, (uint16_t)type, (uint16_t)major, (uint16_t)minor) == NULL)
		return -1;
	return 0;
}

/*
 * 改变进程当前工作目录
 * 用户模式参数：
 * 	path: 新的工作目录；
 * 用户模式返回值：
 * 	成功返回0，失败返回-1；
 */
int32_t sys_chdir(void)
{
	char * path;
	mem_inode_t * ip;

	if(get_str_arg(0, (uint32_t *)&path) <= 0)
		return -1;
	if((ip = resolve_path(path, 0, NULL)) == NULL)
		return -1;

	lock_inode(ip);
	if(ip->type != DIR_INODE)
	{
		unlock_inode(ip);
		release_inode(ip);
		return -1;
	}
	unlock_inode(ip);
	release_inode(cpu.cur_proc->cwd);
	cpu.cur_proc->cwd = ip;

	return 0;
}

/*
 * 创建管道
 * 用户模式参数：
 * 	p[2]: 由两个文件描述符组成的数组的地址；
 * 用户模式返回值：
 * 	成功返回0，且p[]数组中将写有用于读取和写入管道的文件描述符
 * 		p[0]: 只读文件描述符
 * 		p[1]: 只写文件描述符
 * 	失败返回-1，且p[]数组中数据不会被修改；
 */
extern int32_t create_pipe(file_t ** prfile, file_t ** pwfile);
int32_t sys_pipe(void)
{
	int32_t * p;
	file_t * rfp;
	file_t * wfp;
	int32_t rfd;
	int32_t wfd;
	
	if(get_ptr_arg(0, (uint32_t *)&p, 2 * sizeof(*p)) == -1)
		return -1;
	if(create_pipe(&rfp, &wfp) == -1)
		return -1;
	if((rfd = alloc_fd(rfp)) == -1)
		goto bad;
	if((wfd = alloc_fd(wfp)) == -1)
		goto bad;

	p[0] = rfd;
	p[1] = wfd;
	return 0;

bad:
	if(rfd >= 0)
		cpu.cur_proc->open_files[rfd] = NULL;
	/* 关闭打开文件结构，同时释放管道 */
	close_file(rfp);
	close_file(wfp);
	return -1;
}

