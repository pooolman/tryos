#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../tryos/include/fs.h"
#include "./lib.h"
#include "./inode.h"
#include "./block.h"
#include "./extvars.h"

/*
 * 获取一个新的与指定i结点关联的内存i结点结构
 * 注意：返回的内存i结点结构中没有有效数据，需要读取一次
 */
m_inode_t * acquire_inode(uint32_t inum)
{
	m_inode_t * ip = malloc(sizeof(m_inode_t));
	
	ip->fd = global_fd;
	ip->sb = &global_sb;
	ip->inum = inum;

	return ip;
}

/*
 * 在当前操作的文件系统中寻找第一个空闲的磁盘i结点，返回一个与该i结点对应的内存i结点结构指针。
 */
m_inode_t * alloc_inode(void)
{
	uint8_t mask;
	uint8_t sector[BLOCK_SIZE];

	for(uint32_t b = 0; b < global_sb.block_number; b += BITS_PER_BLOCK)
	{
		/* 读取disk inode bitmap中b这个bit所在block */
		if(raw_read(global_fd, SNUM_OF_INODE_BITMAP(b, global_sb), sector, sizeof(sector)) == -1)
		{
			printf("alloc_inode: read inode bitmap error\n");
			return NULL;
		}
		
		for(uint32_t bi = 0; bi < BITS_PER_BLOCK && b+bi < global_sb.block_number; bi++)
		{
			mask = 1 << (bi % 8);
			if((sector[bi / 8] & mask) == 0)
			{
				/* 此时找到了一个空闲bit */
				sector[bi / 8] |= mask;

				/* 写回位图 */
				if(raw_write(global_fd, SNUM_OF_INODE_BITMAP(b+bi, global_sb), sector, sizeof(sector)) == -1)
				{
					printf("alloc_inode: write inode bitmap error\n");
					return NULL;
				}
				
				return acquire_inode(b+bi);
			}
		}
	}
	printf("alloc_inode: no free inodes\n");
	return NULL;
}

/*
 * 读取磁盘i结点到内存i结点结构中，成功返回0，失败返回-1。
 */
int32_t get_inode(m_inode_t * ip)
{
	uint8_t buf[BLOCK_SIZE];
	disk_inode_t * dip;

	/* 读取该inode所在的block到buf中 */
	if(raw_read(ip->fd, SNUM_OF_INODE(ip->inum, *(ip->sb)), buf, BLOCK_SIZE) != 0)
	{
		printf("get_inode: read inode failed\n");
		return -1;
	}

	/* 定位到该inode位置 */
	dip = (disk_inode_t *)buf + ip->inum % INODES_PER_BLOCK;
	/* 读取 */
	ip->type = dip->type;
	ip->major = dip->major;
	ip->minor = dip->minor;
	ip->link_number = dip->link_number;
	ip->size = dip->size;
	memmove(ip->addrs, dip->addrs, sizeof(dip->addrs));

	return 0;
}

/*
 * 将内存i结点中磁盘i结点内容副本写入磁盘对应位置，成功返回0，失败返回-1。
 */
int32_t update_inode(m_inode_t * ip)
{
	uint8_t buf[BLOCK_SIZE];
	disk_inode_t * dip;
	
	/* 读取inode所在block */
	if(raw_read(ip->fd, SNUM_OF_INODE(ip->inum, *(ip->sb)), buf, BLOCK_SIZE) != 0)
	{
		printf("update_inode: read inode failed\n");
		return -1;
	}
	
	/* 定位到该inode位置 */
	dip = (disk_inode_t *)buf + ip->inum % INODES_PER_BLOCK;
	/* 写入 */
	dip->type = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->link_number = ip->link_number;
	dip->size = ip->size;
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	if(raw_write(ip->fd, SNUM_OF_INODE(ip->inum, *(ip->sb)), buf, BLOCK_SIZE) != 0)
	{
		printf("update_inode: write inode failed\n");
		return -1;
	}

	return 0;
}

/*
 * 查找i结点所关联的第n个block，如果不存在则分配新的清零过的block并建立必要的映射，
 * 然后返回其扇区编号，n从0计算。
 * 根据设计，0号sector保留，所以查找失败返回0。
 */
uint32_t get_inode_map(m_inode_t * ip, uint32_t n)
{
	/* bnum为所要查找的block的编号 */
	uint32_t bnum;
	uint8_t buf[BLOCK_SIZE];
	uint32_t * dp;


	if(n < DIRECT_BLOCK_NUMBER)
	{
		/* 在直接索引范围内 */
		if((bnum = ip->addrs[n]) >= ip->sb->block_number || bnum == 0)
		{
			/* 但是n指定的索引无效 */
			bnum = ip->addrs[n] = alloc_block(ip->fd, ip->sb);
			if(bnum == _NAVL_BLK_NUM_)
			{
				printf("get_inode_map: alloc zeroed block failed\n");
				return 0;
			}
			if(update_inode(ip) != 0)
			{
				printf("get_inode_map: update inode failed\n");
				return 0;
			}
		}
		/* DEBUG */
		//printf("get_inode_map: bnum %u\n", bnum);

		return SNUM_OF_BLOCK(bnum, *(ip->sb));
	}

	n -= DIRECT_BLOCK_NUMBER;

	if(n < INDIRECT_BLOCK_NUMBER)
	{
		/* 在间接索引范围内 */
		if(ip->addrs[DIRECT_BLOCK_NUMBER] >= ip->sb->block_number || ip->addrs[DIRECT_BLOCK_NUMBER] == 0)
		{
			/* 但是不存在间接索引块 */
			ip->addrs[DIRECT_BLOCK_NUMBER] = alloc_block(ip->fd, ip->sb);
			if(ip->addrs[DIRECT_BLOCK_NUMBER] == _NAVL_BLK_NUM_)
			{
				printf("get_inode_map: alloc indirect index block failed\n");
				return 0;
			}
			if(update_inode(ip) != 0)
			{
				printf("get_inode_map: update inode failed\n");
				return 0;
			}
		}
		/* 读取间接索引块 */
		if(raw_read(ip->fd, SNUM_OF_BLOCK(ip->addrs[DIRECT_BLOCK_NUMBER], *(ip->sb)), buf, BLOCK_SIZE) != 0)
		{
			printf("get_inode_map: read indirect index block failed\n");
			return 0;
		}
		dp = (uint32_t *)buf;
		if((bnum = dp[n]) >= ip->sb->block_number || bnum == 0)
		{
			/* 但n指定的索引无效 */
			bnum = dp[n] = alloc_block(ip->fd, ip->sb);
			if(bnum == _NAVL_BLK_NUM_)
			{
				printf("get_inode_map: alloc block failed\n");
				return 0;
			}
			if(raw_write(ip->fd, SNUM_OF_BLOCK(ip->addrs[DIRECT_BLOCK_NUMBER], *(ip->sb)), buf, BLOCK_SIZE) != 0)
			{
				printf("get_inode_map: update indirect index block failed\n");
				return 0;
			}
		}
		/* DEBUG */
		//printf("get_inode_map: bnum %u\n", bnum);

		return SNUM_OF_BLOCK(bnum, *(ip->sb));
	}

	printf("get_inode_map: n out of range\n");
	return 0;
}

/*
 * 从磁盘i结点特定偏移处读取指定数量字节到缓冲区中，返回实际读取字节数，出错则返回-1。
 * ip: 指向被读取的i节点
 * dst: 目标缓存区
 * off: i节点中的偏移
 * n: 读取的字节数
 *
 * 注意：
 * 在磁盘i结点结构定义中size成员为无符号的，但实际受限于设计，最大文件仅为70KB，
 * 这里返回值使用有符号类型（为能区别错误值），但仍能满足使用，使用时需要注意。
 * 设备类型的i结点会如同普通文件/目录类型的i节点一样被操作。
 */
int32_t read_inode(m_inode_t * ip, void * dst, uint32_t off, uint32_t n)
{
	int32_t actual_read_bytes;
	uint32_t m;
	uint8_t buf[BLOCK_SIZE];

	if(off > ip->size || off + n < off)
	{
		printf("read_inode: arguments error\n");
		return -1;
	}
	if(off + n > ip->size)
		n = ip->size - off;
	
	actual_read_bytes = (int32_t)n;
	
	for(; n > 0; n -= m, off += m, dst += m)
	{
		uint32_t snum;
		if((snum = get_inode_map(ip, off/BLOCK_SIZE)) == 0)
		{
			printf("read_inode: get sector num of n'th block failed\n");
			return -1;
		}
		if(raw_read(ip->fd, snum, buf, BLOCK_SIZE) != 0)
		{
			printf("read_inode: can not read block\n");
			return -1;
		}
		m = min(n, BLOCK_SIZE - off % BLOCK_SIZE);
		memmove(dst, buf + off % BLOCK_SIZE, m);
	}
	
	return actual_read_bytes;
}

/*
 * 将缓冲区中的指定数量字节写入磁盘i结点特定偏移处，返回实际写入字节数，失败返回-1。
 * ip: 指向被写入的i节点
 * src: 源缓存区
 * off: i节点中的偏移
 * n: 写入的字节数
 *
 * 注意：
 * 当写入出错时（比如off/n有误、写入数据会超出最大文件大小）返回-1。
 * 在磁盘i结点结构定义中size成员为无符号的，但实际受限于设计，最大文件仅为70KB，这里返回值使用有符号类型（为能区别错误值），但仍能满足使用，使用时需要注意。
 * 设备类型的i结点会如同普通文件/目录类型的i节点一样被操作。
 */
int32_t write_inode(m_inode_t * ip, void * src, uint32_t off, uint32_t n)
{
	int32_t actual_write_bytes;
	uint8_t buf[BLOCK_SIZE];
	uint32_t m;

	/* 写入文件时不能超出文件最大大小 */
	if(off > MAX_FILE_SIZE || off + n < off)
	{
		printf("write_inode: arguments error\n");
		return -1;
	}
	if(off + n > MAX_FILE_SIZE)
	{
		printf("write_inode: exceed MAX_FILE_SIZE\n");
		return -1;
	}

	actual_write_bytes = (int32_t)n;
	
	for(; n > 0; n -= m, off += m, src += m)
	{
		uint32_t snum;
		if((snum = get_inode_map(ip, off / BLOCK_SIZE)) == 0)
		{
			printf("write_inode: map block of inode failed\n");
			return -1;
		}
		if(raw_read(ip->fd, snum, buf, BLOCK_SIZE) != 0)
		{
			printf("write_inode: read block failed\n");
			return -1;
		}
		m = min(n, BLOCK_SIZE - off % BLOCK_SIZE);
		memmove(buf + off % BLOCK_SIZE, src, m);
		if(raw_write(ip->fd, snum, buf, BLOCK_SIZE) != 0)
		{
			printf("write_inode: write block failed\n");
			return -1;
		}
	}

	/* 如果发生了写入且写入后off超出了文件大小，则进行更新 */
	if(actual_write_bytes > 0 && off > ip->size)
	{
		ip->size = off;
		if(update_inode(ip) != 0)
		{
			printf("write_inode: update inode failed\n");
			return -1;
		}
	}

	return actual_write_bytes;
}
