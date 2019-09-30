/*
 * 块分配层
 */
#include <stdint.h>
#include "fs.h"
#include "block.h"
#include "string.h"
#include "debug.h"
#include "buf_cache.h"

/*
 * 在指定设备上读取super block
 */
void read_sb(int32_t dev, super_block_t * sb)
{
	buf_t * buf;
	
	buf = acquire_buf(dev, 1);
	memcpy(sb, buf->data, sizeof(super_block_t));
	release_buf(buf);
}

/*
 * 清空指定设备上文件系统中某个block中的内容
 */
static void blk_zero(int32_t dev, super_block_t * sbp, uint32_t bnum)
{
	buf_t * buf;
	
	buf = acquire_buf(dev, SNUM_OF_BLOCK(bnum, *sbp));
	memset(buf->data, 0, sizeof(buf->data));
	write_buf(buf);
	release_buf(buf);
}

/*
 * 在指定块设备上的文件系统中分配空闲块，清空内容，返回其编号
 * 失败则PANIC
 */
uint32_t alloc_block(int32_t dev)
{
	buf_t * buf;
	super_block_t sb;
	uint8_t mask;

	read_sb(dev, &sb);

	for(uint32_t b = 0; b < sb.block_number; b += BITS_PER_BLOCK)
	{
		buf = acquire_buf(dev, SNUM_OF_BLK_BITMAP(b, sb));
		for(uint32_t bi = 0; bi < BITS_PER_BLOCK && b+bi < sb.block_number; bi++)
		{
			mask = 1 << (bi % 8);
			if((buf->data[bi / 8] & mask) == 0)
			{
				/* 此时找到了一个空闲bit */
				buf->data[bi / 8] |= mask;
				write_buf(buf);
				release_buf(buf);
				blk_zero(dev, &sb, b+bi);
				return (b + bi);
			}
		}
		release_buf(buf);
	}

	PANIC("alloc_block: no free blocks");
}

/*
 * 在指定块设备上的文件系统上回收空闲块。
 */
void free_block(int32_t dev, uint32_t bnum)
{
	buf_t * buf;
	super_block_t sb;
	uint8_t mask;

	/* DEBUG */
	iprint_log("free_block: start to free dev %d bnum %u\n", dev, bnum);

	read_sb(dev, &sb);

	buf = acquire_buf(dev, SNUM_OF_BLK_BITMAP(bnum, sb));

	bnum = bnum % BITS_PER_BLOCK;
	mask = 1 << (bnum % 8);
	if((buf->data[bnum / 8] & mask) == 0)
		PANIC("free_block: block bitmap maybe in uncoincident state");
	buf->data[bnum / 8] &= ~mask;

	write_buf(buf);
	release_buf(buf);
}
