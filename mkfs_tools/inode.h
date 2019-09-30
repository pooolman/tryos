#ifndef _INCLUDE_INODE_H_
#define _INCLUDE_INODE_H_

#include <stdint.h>
#include "../tryos/include/fs.h"

/* 内存i节点定义 */
typedef struct{
	int fd; //当前正在操作的文件
	super_block_t * sb; //指向一个公共的超级块
	uint32_t inum; //该结构对应的磁盘i结点

	/* 磁盘i结点内容副本 */
	uint16_t type;		//inode类型
	uint16_t major;		//当inode指代设备时，使用major/minor指定何种设备
	uint16_t minor;
	uint16_t link_number;	//多少个目录项指向了该inode
	uint32_t size;		//文件大小，字节单位
	uint32_t addrs[DIRECT_BLOCK_NUMBER + 1]; //与该inode相关的数据块索引，最后一个作为间接索引
} m_inode_t;

m_inode_t * acquire_inode(uint32_t inum);

m_inode_t * alloc_inode(void);

int32_t get_inode(m_inode_t * ip);

int32_t update_inode(m_inode_t * ip);

uint32_t get_inode_map(m_inode_t * ip, uint32_t n);

int32_t read_inode(m_inode_t * ip, void * dst, uint32_t off, uint32_t n);

int32_t write_inode(m_inode_t * ip, void * src, uint32_t off, uint32_t n);

#endif // _INCLUDE_INODE_H_
