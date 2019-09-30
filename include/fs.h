/*
 * 与文件系统有关的结构定义
 */

#ifndef _INCLUDE_FS_H_
#define _INCLUDE_FS_H_

#include <stdint.h>

/* 文件系统超级块结构 */
typedef struct {
	uint32_t blks_ibitmap; //disk inode bitmap占用的block数
	uint32_t blks_bbitmap; //block bitmap占用的block数
	uint32_t inode_number;	//可用于分配的inode总数，即disk inode bitmap中有效位的个数
	uint32_t block_number;	//可用于分配的block总数，即block bitmap中有效位的个数
	uint32_t blks_inode;	//磁盘i节点占用的块数
} __attribute__((packed)) super_block_t;

/* 磁盘inode类型 */
#define CHR_DEV_INODE	1
#define FILE_INODE	2
#define DIR_INODE	3
#define BLK_DEV_INODE	4

/* block大小，字节单位 */
#define BLOCK_SIZE	512
/* inode直接数据块个数 */
#define DIRECT_BLOCK_NUMBER	12
/* inode间接数据块个数 */
#define INDIRECT_BLOCK_NUMBER	(BLOCK_SIZE/sizeof(uint32_t))


/* 磁盘inode结构 */
typedef struct {
	uint16_t type;		//inode类型
	uint16_t major;		//当inode指代设备时，使用major/minor指定何种设备
	uint16_t minor;
	uint16_t link_number;	//多少个目录项指向了该inode
	uint32_t size;		//文件大小，字节单位
	uint32_t addrs[DIRECT_BLOCK_NUMBER + 1]; //与该inode相关的数据块索引，最后一个作为间接索引
} __attribute__((packed)) disk_inode_t;

/* 每个block(sector)能容纳的磁盘i节点个数 */
#define INODES_PER_BLOCK	(BLOCK_SIZE/sizeof(disk_inode_t))


/* 第n个block对应的sector号，sb为超级块变量 */
#define SNUM_OF_BLOCK(n, sb) (2 + (sb).blks_bbitmap + (sb).blks_ibitmap + (sb).blks_inode + (n))

/* 第n个disk inode所在的sector */
#define SNUM_OF_INODE(n, sb) (2 + (sb).blks_bbitmap + (sb).blks_ibitmap + (n)/INODES_PER_BLOCK)

/* 每一个block(sector)的bit数 */
#define BITS_PER_BLOCK		(BLOCK_SIZE*8)

/* block bitmap中包含bit n的block的sector num */
#define SNUM_OF_BLK_BITMAP(n, sb) (2 + (sb).blks_ibitmap + (n)/BITS_PER_BLOCK)

/* disk inode bitmap中包含bit n的block的sector num */
#define SNUM_OF_INODE_BITMAP(n, sb) (2 + (n)/BITS_PER_BLOCK)

/* 0号block不可用，构建文件系统时会预置该位；便于初始化disk_inode_t.addrs成员 */
#define NAVL_BLK_NUM	0

/* 最大文件大小，字节单位 */
#define MAX_FILE_SIZE	(12*512 + 512/sizeof(uint32_t)*512)

/* i节点结构的使用状态 */
#define INODE_BUSY	0x1	//表示已经被某个进程锁住
#define INODE_VALID	0x2	//表示其内容有效（指磁盘i结点内容副本）

/* 内存中的i节点，包含磁盘inode内容副本以及一些控制信息 */
typedef struct {
	int32_t dev; //i节点从哪个设备上被读出，为与buf_cache中定义一致，所以为int32_t
	uint32_t inum; //为disk inode bitmap中对应bit的偏移量
	uint32_t ref; //表示有多少个对该i结点的引用（指针）
	uint32_t flags; //表示该i结点结构的使用状态；为0表示未设置任何标志
	
	/* 磁盘i节点内容副本 */
	uint16_t type;		//inode类型
	uint16_t major;		//当inode指代设备时，使用major/minor指定何种设备
	uint16_t minor;
	uint16_t link_number;	//多少个目录项指向了该inode
	uint32_t size;		//文件大小，字节单位
	uint32_t addrs[DIRECT_BLOCK_NUMBER + 1]; //与该inode相关的数据块索引，最后一个作为间接索引
	
} mem_inode_t;


/* 目录项最大名字长度，字节计算 */
#define MAX_DIR_NAME_LEN	28
/* 根目录所在i结点号 */
#define ROOT_INUM	0
/* 根文件系统所在设备的设备号 */
#define ROOT_DEV_NO	0

/* 目录项结构，文件大小必须能够容纳整数个目录项 */
typedef struct {
	char name[MAX_DIR_NAME_LEN]; //目录项名字
	uint32_t inum;	//目录项关联的i节点号
} dirent_t;


#endif //_INCLUDE_FS_H_
