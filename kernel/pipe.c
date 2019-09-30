#include <stdint.h>
#include <stddef.h>

#include "pipe.h"
#include "file.h"
#include "process.h"
#include "debug.h"
#include "vmm.h"
#include "fcntl.h"
#include "string.h"

/*
 * 创建管道，并为之分配两个关联的打开文件结构用作读写：
 * prfile：与管道读取端关联的打开文件结构指针的地址
 * pwfile：与管道写入端关联的打开文件结构指针的地址
 *
 * 成功将创建一个管道，并使两个打开文件结构分别作为管道的读写端
 * ，其地址分别写入*prfile和*pwfile中，*prfile用于从管道中读取
 * 数据，*pwfile用于向管道写入数据，然后返回0；失败则返回-1
 */

int32_t create_pipe(file_t ** prfile, file_t ** pwfile)
{
	pipe_t * pp;
	file_t * fp0;
	file_t * fp1;

	/* 分配一页保存管道结构 */
	if((pp = (pipe_t *)alloc_page()) == NULL)
		return -1;
	
	/* 初始化pipe结构 */
	memset(pp, 0, sizeof(*pp));
	pp->ropen = 1;
	pp->wopen = 1;
	
	/* 分配两个关联的打开文件结构并初始化 */
	if((fp0 = alloc_file()) == NULL)
		goto bad;
	if((fp1 = alloc_file()) == NULL)
		goto bad;

	fp0->type = FD_TYPE_PIPE;
	fp0->mode = O_RDONLY;
	fp0->ip = NULL;
	fp0->pipe = pp;
	fp0->off = 0;
	fp1->type = FD_TYPE_PIPE;
	fp1->mode = O_WRONLY;
	fp1->ip = NULL;
	fp1->pipe = pp;
	fp1->off = 0;

	*prfile = fp0;
	*pwfile = fp1;

	return 0;

bad:
	if(fp0)
		close_file(fp0);
	if(fp1)
		close_file(fp1);
	if(pp)
		free_page(pp);
	return -1;
}

/*
 * 关闭管道：
 * pipe: 管道指针；
 * port_type: 被关闭管道的端口，以打开文件结构的操作模式为参数
 * ，指定只读表示关闭读取端，指定只写表示关闭写入端，指定读写则会PANIC；
 *
 * 关闭管道指定的端口，并唤醒等待在另一个端口上的进程，如果所
 * 有端口都已经被关闭，则释放该管道所使用的页面；无返回值。
 */
void close_pipe(pipe_t * pp, uint32_t port_type)
{
	pushcli();
	if((port_type & MODE_RW_MASK) ==  O_RDONLY)
	{
		pp->ropen = 0;
		wakeup(&pp->widx);
	}
	else if((port_type & MODE_RW_MASK) == O_WRONLY)
	{
		pp->wopen = 0;
		wakeup(&pp->ridx);
	}
	else
		PANIC("close_pipe: port type error");
	
	if(pp->ropen == 0 && pp->wopen == 0)
	{
		/* 如果管道的两个端口都已经被关闭，则释放该管道 */
		free_page(pp);
	}
	popcli();
}

/*
 * 从管道中读取n字节数据到buf中：
 * pipe: 管道指针；
 * n: 需要读取的字节数；
 * buf: 写入的起始位置；
 *
 * 成功返回实际读取的字节数（总是大于0），失败返回-1；可能会睡眠下去，具体参考设计文档和实现；
 */
int32_t read_pipe(pipe_t * pp, void * buf, uint32_t n)
{
	int32_t i;

	pushcli();
	if(pp->ropen == 0)
		PANIC("read_pipe: unreadable pipe");
	
	while(pp->ridx == pp->widx && pp->wopen)
	{
		/* 空的管道，且可能会有数据写入 */
		wakeup(&pp->widx);
		sleep(&pp->ridx);
	}
	for(i = 0; (uint32_t)i < n; i++)
	{
		if(pp->ridx == pp->widx)
			break;
		*((uint8_t *)buf + i ) = pp->buf[pp->ridx++ % PIPE_BUF_SIZE];
	}
	
	wakeup(&pp->widx);
	popcli();
	
	if(i == 0)
	{
		/* 没有读取到数据且管道的写端口已经被关闭 */
		return -1;
	}
	return i;
}

/*
 * 向管道写入从buf开始的n字节数据：
 * pipe: 管道指针；
 * n：需要写入的字节数；
 * buf: 待写入的数据；
 *
 * 成功返回实际写入的字节数（总是等于n），失败返回-1；可能会睡眠下去，具体参考设计文档和实现；
 */
int32_t write_pipe(pipe_t * pp, void * buf, uint32_t n)
{
	int32_t i;

	pushcli();
	if(pp->wopen == 0)
		PANIC("write_pipe: unwritable pipe");
	
	for(i = 0; (uint32_t)i < n; i++)
	{
		while(pp->ridx + PIPE_BUF_SIZE == pp->widx)
		{
			if(pp->ropen == 0)
			{
				popcli();
				return -1;
			}
			wakeup(&pp->ridx);
			sleep(&pp->widx);
		}
		if(pp->ropen == 0)
		{
			popcli();
			return -1;
		}
		pp->buf[pp->widx++ % PIPE_BUF_SIZE] = *((uint8_t *)buf + i);
	}
	
	wakeup(&pp->ridx);
	popcli();
	return (int32_t)n;
}

