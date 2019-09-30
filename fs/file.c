/*
 * 打开文件/文件描述符层
 */

#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "fs.h"
#include "inode.h"
#include "stat.h"
#include "file.h"
#include "parameters.h"
#include "process.h"
#include "fcntl.h"

/* 内核维护的打开文件表 */
file_t open_file_table[OPEN_FILE_NUM];

/*
 * 从打开文件表中分配一个空闲结构以供使用，返回结构指针，其引用计数将置为1，ip/pipe指针置为NULL，类型为0（不代表任何资源类型）
 * 注意：这个函数没有设置结构内容，需调用者处理；
 */
file_t * alloc_file(void)
{
	pushcli();
	
	for(int32_t i = 0; i < OPEN_FILE_NUM; i++)
	{
		if(open_file_table[i].ref == 0)
		{
			open_file_table[i].ref = 1;
			open_file_table[i].ip = NULL;
			open_file_table[i].pipe = NULL;
			open_file_table[i].type = 0;
			popcli();
			return &open_file_table[i];
		}
	}
	
	PANIC("alloc_file: no struct avaiable");
}

/*
 * 为当前进程分配一个最小未被使用的文件描述符，并使之引用一个打开文件结构，
 * 成功返回所分配的文件描述符，失败返回-1
 */
int32_t alloc_fd(file_t * fp)
{
	for(int32_t i = 0; i < PROC_OPEN_FD_NUM; i++)
	{
		if(cpu.cur_proc->open_files[i] == NULL)
		{
			cpu.cur_proc->open_files[i] = fp;
			return i;
		}
	}
	return -1;
}

/*
 * 丢弃一个打开文件结构的引用，必要时丢弃结构中对inode的引用或者释放管道
 */
void close_file(file_t * fp)
{
	file_t tmp;
	
	pushcli();
	if(fp->ref < 1)
		PANIC("close_file: not a valid reference");
	if(--fp->ref > 0) //减一之后是否仍大于0
	{
		/* 说明还有其他的有效引用 */
		popcli();
		return;
	}
	/* 这已经是最后一个引用了 */
	tmp = *fp; //尽快释放这个结构以便其他进程使用
	fp->ip = NULL; //保险起见，置为NULL
	fp->pipe = NULL; //保险起见，置为NULL
	popcli();

	if(tmp.ip)
		release_inode(tmp.ip); //可能会删除这个文件
	else if(tmp.pipe)
		close_pipe(tmp.pipe, tmp.mode); //关闭管道的这一端口，可能会释放这个管道
}

/*
 * 复制一个对打开文件的引用，增加其引用计数，返回新的引用
 */
file_t * dup_file(file_t * fp)
{
	pushcli();
	if(fp->ref < 1)
		PANIC("dup_file: not a valid reference");
	fp->ref++;
	popcli();
	return fp;
}

/*
 * 在一个打开文件结构上从当前文件偏移处开始读取最多n个字节到buf中，
 * 成功则文件偏移量增加实际读取字节数，并返回实际读取字节数，
 * 失败返回-1，不修改文件偏移量。
 * 如果所表示的是管道则其行为取决于管道的定义。
 */
int32_t read_file(file_t * fp, void * buf, uint32_t n)
{
	int32_t ret;

	/* 这里没有关闭中断进行检查，但是仍然可能检查出一些错误 */
	if(fp->ref < 1)
		PANIC("read_file: not a valid reference");
	
	/* 检查是否允许读 */
	if((fp->mode & MODE_RW_MASK) != O_RDWR && (fp->mode & MODE_RW_MASK) != O_RDONLY)
		return -1;

	switch(fp->type)
	{
		case FD_TYPE_PIPE:
			if(fp->pipe == NULL)
				PANIC("read_file: no reference to pipe");
			return read_pipe(fp->pipe, buf, n);
		case FD_TYPE_INODE:
			if(fp->ip == NULL)
				PANIC("read_file: no reference to inode");
	
			lock_inode(fp->ip);
			if((ret = read_inode(fp->ip, buf, fp->off, n)) > 0)
				fp->off += ret;
			unlock_inode(fp->ip);
			return ret;
		default:
			PANIC("read_file: unknown file type");
	}

}

/*
 * 在一个打开文件结构上从当前文件偏移处开始写入从buf开始的n个字节，
 * 成功返回写入字节数n，且文件偏移量增加n个字节，失败返回-1，不修改文件偏移量。
 * 如果所表示的是管道则其行为取决于管道的定义。
 */
int32_t write_file(file_t * fp, void * buf, uint32_t n)
{
	int32_t ret;

	/* 这里没有关闭中断进行检查，但是仍然可能检查出一些错误 */
	if(fp->ref < 1)
		PANIC("write_file: not a valid reference");
	
	/* 检查是否允许写 */
	if((fp->mode & MODE_RW_MASK) != O_RDWR && (fp->mode & MODE_RW_MASK) != O_WRONLY)
		return -1;

	switch(fp->type)
	{
		case FD_TYPE_PIPE:
			if(fp->pipe == NULL)
				PANIC("write_file: unvalid pipe pointer");
			return write_pipe(fp->pipe, buf, n);
		case FD_TYPE_INODE:
			if(fp->ip == NULL)
				PANIC("write_file: no reference to inode");
			lock_inode(fp->ip);
			if((ret = write_inode(fp->ip, buf, fp->off, n)) > 0 && (uint32_t)ret == n)
				fp->off += n;
			unlock_inode(fp->ip);
			return ret;
		default:
			PANIC("write_file: unknown file type");
	}
}

/*
 * 读取与打开文件结构关联的i结点信息到stat结构中。
 * 如果所表示的不是i结点，则返回-1，否则返回0。
 */
int32_t stat_file(file_t * fp, stat_t * st)
{
	/* 这里没有关闭中断进行检查，但是仍然可能检查出一些错误 */
	if(fp->ref < 1)
		PANIC("stat_file: not a valid reference");

	if(fp->type != FD_TYPE_INODE)
		return -1;
	if(fp->ip == NULL)
		PANIC("stat_file: unvalid inode pointer");
	lock_inode(fp->ip);
	stat_inode(fp->ip, st);
	unlock_inode(fp->ip);
	return 0;
}


/* DEBUG */
/* 输出打开文件结构中的信息，不检查其是否有效 */
void dump_file(file_t * fp)
{
	pushcli();

	print_log("open_file_struct: ref: %hu", fp->ref);

	switch(fp->type)
	{
		case FD_TYPE_PIPE:
			print_log(" pipe,");
			break;
		case FD_TYPE_INODE:
			print_log(" inode,");
			break;
		default:
			print_log(" unkonwn type,");
	}
	
	switch(fp->mode & MODE_RW_MASK)
	{
		case O_RDONLY:
			print_log(" RD_ONLY, ");
			break;
		case O_WRONLY:
			print_log(" WR_ONLY, ");
			break;
		case O_RDWR:
			print_log(" RD_WR, ");
			break;
		default:
			print_log(" UNKNOWN, ");
	}
	print_log("off: %u\n", fp->off);
	print_log("  releated inode: ");
	dump_inode(fp->ip);

	popcli();
}
