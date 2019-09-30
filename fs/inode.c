/*
 * i节点层实现
 */
#include <stdint.h>
#include <stddef.h>
#include "debug.h"
#include "string.h"
#include "parameters.h"
#include "fs.h"
#include "block.h"
#include "buf_cache.h"
#include "stat.h"
#include "process.h"
#include "inode.h"

/* 字符设备表 */
chr_dev_opts_t chr_dev_opts_table[CHR_DEV_COUNT];

/* inode cache */
static mem_inode_t inode_cache[CACHE_INODE_NUM];

/*
 * 在inode cache中查找或者新分配一个对应于指定设备上某个i结点的inode结构。
 * 注意：
 * 不同于acquire_buf，使用inode前必须先锁住，才能够保证只有当前进程使用该结构且其数据和磁盘一致。
 * 这里没有检查设备号和i结点编号。
 */
mem_inode_t * acquire_inode(int32_t dev, uint32_t inum)
{
	mem_inode_t * empty_ip = NULL; //指向第一个被找到的空的inode结构
	
	pushcli();

	/* 遍历inode cache */
	for(int32_t i = 0; i < CACHE_INODE_NUM; i++)
	{
		if(inode_cache[i].ref > 0 && inode_cache[i].dev == dev && inode_cache[i].inum == inum)
		{
			/* 目标i节点已经被cache了 */
			inode_cache[i].ref++;
			popcli();
			return &inode_cache[i];
		}
		if(empty_ip == NULL && inode_cache[i].ref == 0)
			empty_ip = &inode_cache[i];
	}
	
	/* 没有被cache，但是找到一个空的 */
	if(empty_ip != NULL)
	{
		empty_ip->dev = dev;
		empty_ip->inum = inum;
		empty_ip->ref = 1;
		empty_ip->flags = 0;
		
		popcli();
		return empty_ip;
	}

	/* inode cache被占满 */
	PANIC("acquire_inode: no free inode");
}

/*
 * 在指定设备的文件系统上分配磁盘i结点，并返回其对应的内存inode的引用，未上锁。
 */
mem_inode_t * alloc_inode(int32_t dev)
{
	super_block_t sb;
	buf_t * buf;
	uint8_t mask;
	
	read_sb(dev, &sb);
	
	for(uint32_t b = 0; b < sb.inode_number; b += BITS_PER_BLOCK)
	{
		buf = acquire_buf(dev, SNUM_OF_INODE_BITMAP(b, sb));
		for(uint32_t bi = 0; bi < BITS_PER_BLOCK && b+bi < sb.inode_number; bi++)
		{
			mask = 1 << (bi % 8);
			if((buf->data[bi / 8] & mask) == 0)
			{
				/* 找到一个空闲bit */
				buf->data[bi / 8] |= mask;
				write_buf(buf);
				release_buf(buf);
				return acquire_inode(dev, b + bi);
			}
		}
		release_buf(buf);
	}
	
	PANIC("alloc_inode: no free inodes");
}

/*
 * 在指定设备的文件系统上释放磁盘i结点。
 */
static void free_inode(int32_t dev, uint32_t inum)
{
	buf_t * buf;
	super_block_t sb;
	uint8_t mask;
	
	read_sb(dev, &sb);
	
	buf = acquire_buf(dev, SNUM_OF_INODE_BITMAP(inum, sb));
	inum = inum % BITS_PER_BLOCK;
	mask = 1 << (inum % 8);
	if((buf->data[inum / 8] & mask) == 0)
		PANIC("free_inode: inode bitmap maybe in uncoincident state");
	buf->data[inum / 8] &= ~mask;

	write_buf(buf);
	release_buf(buf);
}

/*
 * 对inode上锁，如果数据不可用则还从磁盘获取该inode对应的数据。
 */
void lock_inode(mem_inode_t * ip)
{
	buf_t * buf;
	super_block_t sb;
	disk_inode_t * dip;
	
	pushcli();
	if(ip->ref < 1)
		PANIC("lock_inode: invalid inode pointer");
	/* 如果已经被锁住，则睡眠 */
	while(ip->flags & INODE_BUSY)
		sleep(ip);

	/* 锁上这个inode */
	ip->flags |= INODE_BUSY;
	/* inode已经锁上，不再需要关中断 */
	popcli();

	if( ! (ip->flags & INODE_VALID))
	{
		/* 如果数据和磁盘不一致 */
		read_sb(ip->dev, &sb);
		/* 锁上i节点所在的block */
		buf = acquire_buf(ip->dev, SNUM_OF_INODE(ip->inum, sb));
		/* 复制数据到inode中 */
		dip = (disk_inode_t *)(buf->data) + ip->inum % INODES_PER_BLOCK;
		ip->type = dip->type;
		ip->major = dip->major;
		ip->minor = dip->minor;
		ip->link_number = dip->link_number;
		ip->size = dip->size;
		memmove(ip->addrs, dip->addrs, sizeof(dip->addrs));
		/* 复制完毕，释放buf */
		release_buf(buf);
		ip->flags |= INODE_VALID;
	}
}

/*
 * 将inode中的磁盘i结点内容副本写入磁盘
 * 注意：要求调用者提前锁住ip；
 */
void update_inode(mem_inode_t * ip)
{
	super_block_t sb;
	buf_t * buf;
	disk_inode_t * dip;

	//注意：由于没有关中断，这里对ref的检查并不安全
	//但仍然做该检查，希望避免一些错误
	if(ip->ref < 1 || !(ip->flags & INODE_BUSY))
		PANIC("update_inode: no reference to inode or it's unlocked");
	
	/* 定位到要写入的位置 */
	read_sb(ip->dev, &sb);
	buf = acquire_buf(ip->dev, SNUM_OF_INODE(ip->inum, sb));
	dip = (disk_inode_t *)(buf->data) + ip->inum % INODES_PER_BLOCK;
	/* 写入 */
	dip->type = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->link_number = ip->link_number;
	dip->size = ip->size;
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	write_buf(buf);

	release_buf(buf);
}

/*
 * 解锁inode
 */
void unlock_inode(mem_inode_t * ip)
{
	//注意：由于没有关中断，这里对ref的检查并不安全
	//但仍然做该检查，希望避免一些错误
	if(ip->ref < 1 || !(ip->flags & INODE_BUSY))
		PANIC("unlock_inode: no reference to inode or it's already unlocked");
	
	ip->flags &= ~INODE_BUSY;
	wakeup(ip); //唤醒在该inode上睡眠的进程
}

/*
 * 释放与inode相关联的所有数据块，包括间接索引块(如果存在)
 */
void trunc_inode(mem_inode_t * ip)
{
	super_block_t sb;
	buf_t * buf;
	uint32_t * index;

	//注意：由于没有关中断，这里对ref的检查并不安全
	//但是仍然进行检查，希望能发现一些错误
	if(ip->ref < 1 || !(ip->flags & INODE_BUSY))
		PANIC("trunc_inode: no reference to inode or it's already unlocked");

	read_sb(ip->dev, &sb);
	/* 清除有关连的直接索引的数据块 */
	for(int32_t i = 0; i < DIRECT_BLOCK_NUMBER; i++)
	{
		if(0 < ip->addrs[i] && ip->addrs[i] < sb.block_number)
		{
			free_block(ip->dev, ip->addrs[i]);
			ip->addrs[i] = NAVL_BLK_NUM;
		}
	}

	if(0 < ip->addrs[DIRECT_BLOCK_NUMBER] && ip->addrs[DIRECT_BLOCK_NUMBER] < sb.block_number)
	{
		/* DEBUG */
		iprint_log("turnc_inode: indirect index block num: %u\n", ip->addrs[DIRECT_BLOCK_NUMBER]);

		/* 存在间接索引块 */
		
		/* 读取并锁住间接索引块 */
		buf = acquire_buf(ip->dev, SNUM_OF_BLOCK(ip->addrs[DIRECT_BLOCK_NUMBER], sb));
		
		

		/*DEBUG */
		iprint_log("trunc_inode: has read and lock indirect index block\n");
		iprint_log("trunc_inode: snum of indirect index block: %u\n", SNUM_OF_BLOCK(ip->addrs[DIRECT_BLOCK_NUMBER], sb));
		iprint_log("trunc_inode: dump buf of indirect index block:\n");
		dump_buf(buf);
		

		index = (uint32_t *)(buf->data);

		/* DEBUG */
		iprint_log("trunc_inode: index value: %X\n", index);

		/* 遍历间接索引块，释放关联的所有数据块 */
		for(; index < (uint32_t *)(buf->data + BLOCK_SIZE); index++)
		{
			if(0 < *index && *index < sb.block_number)
			{
				/* DEBUG */
				iprint_log("trunc_inode: start to free block: %u\n", *index);

				free_block(ip->dev, *index);
			}
		}
		
		/* 释放间接索引块上的锁，然后释放这个块 */
		release_buf(buf);
		free_block(ip->dev, ip->addrs[DIRECT_BLOCK_NUMBER]);
		/* 清除间接索引 */
		ip->addrs[DIRECT_BLOCK_NUMBER] = NAVL_BLK_NUM;
	}

	ip->size = 0;
	/* 将更改同步到磁盘 */
	update_inode(ip);
}

/*
 * 丢弃inode的引用，引用计数减一，如果没有目录项指向该inode对应的磁盘i节点，则该磁盘i节点
 * 将会被释放掉（包括关联的blocks）。
 * 注意：丢弃之前必须解锁该inode。
 */
void release_inode(mem_inode_t * ip)
{
	pushcli();

	if(ip->ref < 1)
		PANIC("release_inode: no reference to this inode");
	
	if(ip->ref == 1 && (ip->flags & INODE_VALID) && ip->link_number == 0)
	{
		/* 仅此一个引用且链接数为0，应该回收这个磁盘i节点 */
		/* 不能丢弃一个被锁住的inode */
		if(ip->flags & INODE_BUSY)
			PANIC("release_inode: try to release a locked inode");

		/* 为了后续函数调用，加上BUSY标志 */
		ip->flags |= INODE_BUSY;
		/* 释放所有与该inode关联的数据块，如果有间接索引块，一并释放 */
		trunc_inode(ip);
		/* 释放该inode对应的磁盘i节点 */
		free_inode(ip->dev, ip->inum);
		/* 保险起见，清空标志 */
		ip->flags = 0;
	}
	ip->ref--;

	popcli();
}

/*
 * 复制一个inode结构的引用，返回复制后的引用
 */
mem_inode_t * dup_inode(mem_inode_t * ip)
{
	pushcli();
	if(ip->ref < 1)
		PANIC("dup_inode: no effective refrence to the inode");
	ip->ref++;
	popcli();
	
	return ip;
}

/*
 * 获取inode映射的第n个block对应的扇区编号，如果该位置尚未映射block则新分配一个清零过的block并建立映射关系
 * n从0计算。
 */
static uint32_t get_inode_map(mem_inode_t * ip, uint32_t n)
{
	super_block_t sb;
	buf_t * buf;
	uint32_t bnum;
	uint32_t * dp;

	read_sb(ip->dev, &sb);
	
	if(n < DIRECT_BLOCK_NUMBER)
	{
		/* 在直接索引范围内 */
		if((bnum = ip->addrs[n]) >= sb.block_number || bnum == 0)
		{
			/* 但是n指定的索引无效 */
			bnum = ip->addrs[n] = alloc_block(ip->dev);
			update_inode(ip);
		}
		return SNUM_OF_BLOCK(bnum, sb);
	}

	n -= DIRECT_BLOCK_NUMBER;

	if(n < INDIRECT_BLOCK_NUMBER)
	{
		/* 在间接索引范围内 */
		if(ip->addrs[DIRECT_BLOCK_NUMBER] >= sb.block_number || ip->addrs[DIRECT_BLOCK_NUMBER] == 0)
		{
			/* 但是不存在间接索引块 */
			ip->addrs[DIRECT_BLOCK_NUMBER] = alloc_block(ip->dev);
			update_inode(ip);
		}
		/* 读取并锁住间接索引块 */
		buf = acquire_buf(ip->dev, SNUM_OF_BLOCK(ip->addrs[DIRECT_BLOCK_NUMBER], sb));
		dp = (uint32_t *)(buf->data);
		if((bnum = dp[n]) >= sb.block_number || bnum == 0)
		{
			/* 但n指定的索引无效 */
			bnum = dp[n] = alloc_block(ip->dev);
			write_buf(buf);
		}
		release_buf(buf);
		return SNUM_OF_BLOCK(bnum, sb);
	}

	PANIC("get_inode_map: n out of range");
}

/* 返回a,b中数值最小的那个，用于连续的读取/写入i节点函数 */
#define MIN(a, b) ((a) > (b) ? (b) : (a))

/*
 * 从inode的特定偏移处读取指定数量字节到缓存区中，返回实际读取字节数。
 * 当读取出错时（比如off/n有误）返回-1。
 * ip: 目标inode
 * dst: 目标缓存区
 * off: 偏移量
 * n: 读取的字节数
 * 注意：
 * 在磁盘i结点结构定义中size成员为无符号的，但实际受限于设计，最大文件仅为70KB，这里返回值使用有符号类型(为了能返回合适的错误值)，但仍能满足使用，使用时需要注意。
 * 对于设备的读操作，参照相关设备的说明。
 */
int32_t read_inode(mem_inode_t * ip, void * dst, uint32_t off, uint32_t n)
{
	if(ip->ref < 1 || !(ip->flags & INODE_BUSY))
		PANIC("read_inode: not an effective refrence or the inode is unlocked");
	
	if(ip->type == CHR_DEV_INODE)
	{
		/* 字符设备 */
		return chr_dev_opts_table[ip->major].read(dst, n);
	}
	else if(ip->type == BLK_DEV_INODE)
		PANIC("read_inode: read from block device file is not supported");

	/* 对于目录/普通文件都能读取 */
	if(off > ip->size || off + n < off)
		return -1;
	if(off + n > ip->size) //读取不可超出当前文件大小
		n = ip->size - off;
	
	int32_t actual_read_bytes = (int32_t)n; //实际读取的字节数
	uint32_t m; //本次读取的字节数
	buf_t * buf;
	for(; n > 0; n -= m, off += m, dst += m)
	{
		buf = acquire_buf(ip->dev, get_inode_map(ip, off/BLOCK_SIZE));
		m = MIN(n, BLOCK_SIZE - off % BLOCK_SIZE);
		memmove(dst, &buf->data[off % BLOCK_SIZE], m);
		release_buf(buf);
	}
	
	return actual_read_bytes;
}

/*
 * 向inode的特定偏移处写入缓存区中的指定数量字节，返回实际写入字节数。
 * 当写入出错时（比如off/n有误、写入数据超出最大文件大小）返回-1，此时不会写入任何数据。
 * ip: 待写入的inode
 * src: 源缓存区
 * off: 写入inode的偏移量
 * n: 待写入的字节数
 *
 * 注意：
 * 在磁盘i结点结构定义中size成员为无符号的，但实际受限于设计，最大文件仅为70KB，这里返回值使用有符号类型（为能区别错误值），但仍能满足使用，使用时需要注意。
 * 对于设备的写操作，参照相关设备的说明。
 */
int32_t write_inode(mem_inode_t * ip, void * src, uint32_t off, uint32_t n)
{
	buf_t * buf;
	uint32_t m;
	int32_t actual_write_bytes;

	if(ip->ref < 1 || !(ip->flags & INODE_BUSY))
		PANIC("write_inode: not an effective reference or the inode is unlocked");

	if(ip->type == CHR_DEV_INODE)
	{
		/* 字符设备 */
		return chr_dev_opts_table[ip->major].write(src, n);
	}
	else if(ip->type == BLK_DEV_INODE)
		PANIC("write_inode: write to block device file is not supported");
	
	/* 实现写入目录/普通文件 */
	if(off > MAX_FILE_SIZE || off + n < off)
		return -1;
	if(off + n > MAX_FILE_SIZE)
		return -1;
	
	actual_write_bytes = (int32_t)n;
	for(; n > 0; n -= m, off += m, src += m)
	{
		buf = acquire_buf(ip->dev, get_inode_map(ip, off/BLOCK_SIZE));
		m = MIN(n, BLOCK_SIZE - off % BLOCK_SIZE);
		memmove(&buf->data[off % BLOCK_SIZE], src, m);
		/* 刷新到磁盘 */
		write_buf(buf);
		release_buf(buf);
	}

	/* 发生实际的写入且写入后off超出了当前文件大小时对当前文件大小进行更新。*/
	if(actual_write_bytes > 0 && off > ip->size)
	{
		ip->size = off;
		update_inode(ip);
	}

	return actual_write_bytes;
}

/*
 * 供用户进程读取inode的信息。
 */
void stat_inode(mem_inode_t * ip, stat_t * st)
{
	if(ip->ref < 1 || !(ip->flags & INODE_BUSY))
		PANIC("stat_inode: not an effective reference or the inode is unlocked");
	
	st->dev = ip->dev;
	st->inum = ip->inum;
	st->type = ip->type;
	st->major = ip->major;
	st->minor = ip->minor;
	st->link_number = ip->link_number;
	st->size = ip->size;
}


/*
 * DEBUG
 */
/*
 * 输出ip引用的inode结构信息，不包括与其关联的block内容，也不管其内容是否有效。
 */
void dump_inode(mem_inode_t * ip)
{
	pushcli();

	/* 检查时要考虑ref是否大于0 */
	print_log("[%u]: dev %d ref %u ",
			ip->inum,
			ip->dev,
			ip->ref);
	if(ip->flags & INODE_BUSY)
		print_log("BUSY ");
	if(ip->flags & INODE_VALID)
		print_log("VALID ");
	/* 仅当inode为VALID时以下数据才有效 */
	switch(ip->type)
	{
		case CHR_DEV_INODE:
			print_log("CHR ");
			break;
		case BLK_DEV_INODE:
			print_log("BLK ");
			break;
		case DIR_INODE:
			print_log("DIR ");
			break;
		case FILE_INODE:
			print_log("FILE ");
			break;
		default:
			print_log("UNKNOWN ");
	}
	print_log("<%hu,%hu>, %hu, %u: ",
			ip->major,
			ip->minor,
			ip->link_number,
			ip->size);
	for(int32_t i = 0; i < DIRECT_BLOCK_NUMBER + 1; i++)
		print_log("%u ", ip->addrs[i]);

	print_log("\n");

	popcli();
}
