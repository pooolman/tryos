#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../tryos/include/fs.h"
#include "./lib.h"
#include "./block.h"

/*
 * 在文件系统中分配所找到的第一个空闲数据块。
 * fd必须引用一个为可读可写方式打开的文件，且已经通过make_fs调用格式化过。
 * sb为超级块指针。
 * 成功返回数据块号，失败返回_NAVL_BLK_NUM_ (不同于NAVL_BLK_NUM)
 */
uint32_t alloc_block(int fd, super_block_t * sb)
{
	uint8_t mask;
	uint8_t sector[BLOCK_SIZE];

	for(uint32_t b = 0; b < sb->block_number; b += BITS_PER_BLOCK)
	{
		/* 读取block bitmap中b这个bit所在block */
		if(raw_read(fd, SNUM_OF_BLK_BITMAP(b, *sb), sector, sizeof(sector)) == -1)
		{
			printf("alloc_block: read block bitmap error\n");
			return _NAVL_BLK_NUM_;
		}
		
		for(uint32_t bi = 0; bi < BITS_PER_BLOCK && b+bi < sb->block_number; bi++)
		{
			mask = 1 << (bi % 8);
			if((sector[bi / 8] & mask) == 0)
			{
				/* 此时找到了一个空闲bit */
				sector[bi / 8] |= mask;

				/* 写回位图 */
				if(raw_write(fd, SNUM_OF_BLK_BITMAP(b+bi, *sb), sector, sizeof(sector)) == -1)
				{
					printf("alloc_block: write block bitmap error\n");
					return _NAVL_BLK_NUM_;
				}

				/* 清空所找到的数据块 */
				memset(sector, 0, sizeof(sector));
				if(raw_write(fd, SNUM_OF_BLOCK(b+bi, *sb), sector, sizeof(sector)) == -1)
				{
					printf("alloc_block: clear block error\n");
					return _NAVL_BLK_NUM_;
				}
				return (b + bi);
			}
		}
	}
	printf("alloc_block: no free blocks\n");
	return _NAVL_BLK_NUM_; //没有空闲block了
}

/*
 * 在文件系统中释放一个数据块。
 * fd必须引用一个为可读可写方式打开的文件，且已经通过make_fs调用格式化过。
 * sb为超级块指针。
 * 成功返回0，失败返回-1
 */
int32_t free_block(int fd, super_block_t * sb, uint32_t bnum)
{
	uint8_t mask;
	uint8_t sector[BLOCK_SIZE];


	/* 读取该block对应的位图中的块 */
	if(raw_read(fd, SNUM_OF_BLK_BITMAP(bnum, *sb), sector, sizeof(sector)) == -1)
	{
		printf("free_block: read block bitmap error\n");
		return -1;
	}
	
	/* 修改位图 */
	bnum = bnum % BITS_PER_BLOCK;
	mask = 1 << ((bnum % BITS_PER_BLOCK) % 8);
	sector[(bnum % BITS_PER_BLOCK) / 8] &= ~mask;
	
	/* 写回 */
	if(raw_write(fd, SNUM_OF_BLK_BITMAP(bnum, *sb), sector, sizeof(sector)) == -1)
	{
		printf("free_block: write block bitmap error\n");
		return -1;
	}

	return 0;
}

/*
 * 清零数据块。成功返回0，失败返回-1。
 */
int32_t zero_block(int fd, super_block_t * sb, uint32_t bnum)
{
	uint8_t buf[BLOCK_SIZE];
	
	memset(buf, 0, sizeof(buf));
	if(raw_write(fd, SNUM_OF_BLOCK(bnum, *sb), buf, BLOCK_SIZE) != 0)
	{
		printf("zero_block: write block failed\n");
		return -1;
	}

	return 0;
}
