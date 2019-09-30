/*
 * 本文件提供与块缓冲有关的接口实现
 */

#include <stdint.h>
#include <stddef.h>
#include "debug.h"
#include "buf_cache.h"
#include "ide.h"
#include "process.h"
#include "parameters.h"

#include "terminal_io.h"

struct {
	buf_t buf[BUF_COUNT]; //块缓冲区
	buf_t head; //访问块缓冲区的链表头
}buf_cache;


/*
 * 初始化块缓冲，使得所有的buf形成一个双向链表；初始化之后，所有操作都通过双向链表完成。
 */
void init_buf_cache(void)
{	
	int32_t i;
	
	for(i = BUF_COUNT - 1; i > 0; i--)
	{
		buf_cache.buf[i].next = &buf_cache.buf[i - 1];
		buf_cache.buf[i - 1].prev = &buf_cache.buf[i];
		buf_cache.buf[i].dev = -1;
	}
	
	buf_cache.buf[BUF_COUNT - 1].prev = &buf_cache.head;
	buf_cache.buf[0].next = &buf_cache.head;
	buf_cache.buf[0].dev = -1;
	
	buf_cache.head.prev = &buf_cache.buf[0];
	buf_cache.head.next = &buf_cache.buf[BUF_COUNT - 1];
}


/*
 * 将指定buf移动到链表MRU端
 */
static inline void move_buf(buf_t * buf)
{
	if(buf == buf_cache.head.next)
		return; //此时buf已经处于MRU端，无需移动，直接返回
	
	buf->next->prev = buf->prev;
	buf->prev->next = buf->next;

	buf_cache.head.next->prev = buf;
	buf->next = buf_cache.head.next;
	
	buf->prev = &buf_cache.head;
	buf_cache.head.next = buf;
}


/*
 * 获取一个映射到指定dev/sector上的buf，该buf将移动到MRU的位置且设置为BUSY的。
 * 如果buf已经用完了，目前的处理是PANIC。
 */
static buf_t * get_buf(int32_t dst_dev, uint32_t dst_sector)
{
	buf_t * buf;
	pushcli(); //关闭中断，保存原来的中断状态

	continue_check:
	/* 查看dst_dev/dst_sector是否已经cache了 */
	/* 按照MRU的顺序查找，更容易找到 */
	for(buf = buf_cache.head.next; buf != &buf_cache.head; buf = buf->next)
	{
		if(buf->dev == dst_dev && buf->sector == dst_sector)
		{
			if( ! (buf->flags & BUF_BUSY))
			{
				buf->flags |= BUF_BUSY;
				move_buf(buf);
				
				popcli(); //恢复原来的中断状态
				return buf;
			}
			sleep(buf);
			goto continue_check;
		}
	}

	/* dst_dev/dst_sector尚未cache，则寻找一个空闲的buf */
	/* 按照LRU的顺序查找 */
	for(buf = buf_cache.head.prev; buf != &buf_cache.head; buf = buf->prev)
	{
		if( ! (buf->flags & BUF_BUSY))
		{
			buf->dev = dst_dev;
			buf->sector = dst_sector;
			buf->flags = BUF_BUSY; //只设置BUSY
			move_buf(buf);

			popcli(); //恢复原来的中断状态
			return buf;
		}
	}
	
	/* 没有buf可用了，目前暂时是PANIC */
	PANIC("get_buf: no free buf available");
}


/*
 * 请求一个映射到指定dev/sector上的buf
 */
buf_t * acquire_buf(int32_t dst_dev, uint32_t dst_sector)
{
	buf_t * buf;
	
	/* 获得一个与dst_dev/dst_sector对应的buf，该buf为BUSY的，即被当前进程占有 */
	buf = get_buf(dst_dev, dst_sector);

	/* 该buf中是否有可用数据 ? */
	if( ! (buf->flags & BUF_VALID))
		sync_ide(buf); //没有则同步一次
	
	/* 此时buf是BUSY、VALID、UN-DIRTY的 */
	return buf;
}


/*
 * 同步buf和对应sector
 */
void write_buf(buf_t * buf)
{
	if( !(buf->flags & BUF_BUSY))
		PANIC("write_buf: no process has owned this buf");
	buf->flags |= BUF_DIRTY;
	sync_ide(buf);
}


/*
 * 释放buf
 */
void release_buf(buf_t * buf)
{
	if( ! (buf->flags & BUF_BUSY))
		PANIC("release_buf: no process has owned this buf");
	buf->flags &= ~BUF_BUSY;
	wakeup(buf);
}


/* DEBUG */
void dump_buf(buf_t * buf)
{
	pushcli();

	print_log("%X<-", buf->prev);
	print_log("(%X: %d,%u: %X: ", buf, buf->dev, buf->sector, buf->data);
	if(buf->flags & BUF_BUSY)
		print_log("BUSY, ");
	if(buf->flags & BUF_VALID)
		print_log("VALID, ");
	if(buf->flags & BUF_DIRTY)
		print_log("DIRTY, ");
	print_log("qnext: %X)", buf->qnext);
	print_log("->%X\n", buf->next);

	popcli();
}

void dump_buf_cache(void)
{
	buf_t * buf;

	for(buf = buf_cache.head.next; buf != &buf_cache.head; buf = buf->next)
	{
		dump_buf(buf);
	}
}
