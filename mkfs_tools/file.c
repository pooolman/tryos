#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../tryos/include/fs.h"
#include "inode.h"
#include "path.h"

/*
 * 在指定目录下创建名为name的目录项，其指向一个长度为0的i结点文件；成功返回该i结点文件的内存i结点指针（已经含有有效数据），失败返回NULL。
 *
 * 注意：
 * 该函数能够创建目录文件、普通文件、设备文件，新创建的目录文件默认仅具有"."和".."这两个目录项，
 * 对于设备文件，并未设置其major/minor号，需调用者进一步处理；
 * 该函数不同于linux中的creat函数，当文件存在时创建操作将失败；
 */

m_inode_t * create(m_inode_t * dp, uint16_t type, const char * name)
{
	m_inode_t * ip = NULL;

	if(dp->type != DIR_INODE)
	{
		printf("create: not a directory");
		return NULL;
	}
	
	/* 检查是否存在同名目录项 */
	int32_t ret;
	if((ret = lookup_dir(dp, name, &ip)) == -1)
	{
		printf("create: check directory failed\n");
		return NULL;
	}
	if(ip)
	{
		printf("create: directory item already exist\n");
		return NULL;
	}

	/* 分配一个新的i节点，获取与之关联的内存i节点结构 */
	if((ip = alloc_inode()) == NULL)
	{
		printf("create: alloc inode failed\n");
		return NULL;
	}
	
	/* 构建i节点文件 */
	ip->type = type;
	ip->link_number = 1;
	ip->size = ip->major = ip->minor = 0;
	memset(ip->addrs, 0, sizeof(ip->addrs));

	/* 如果新建的是目录，则还需要建立"."和".."这两个目录项 */
	if(ip->type == DIR_INODE)
	{
		if(add_link(ip, ".", ip->inum) == -1 || add_link(ip, "..", dp->inum) == -1)
		{
			printf("create: add ./.. item to directory failed\n");
			free(ip);
			return NULL;
		}
		ip->link_number++;
		dp->link_number++;
		if(update_inode(dp) == -1)
		{
			printf("create: update parent dir failed\n");
			return NULL;
		}
	}
	
	/* 在父目录中添加这个目录项 */
	if(add_link(dp, name, ip->inum) == -1)
	{
		printf("create: add link to parent dir failed\n");
		free(ip);
		return NULL;
	}
	/* 写回 */
	if(update_inode(ip) == -1)
	{
		printf("create: update new inode failed\n");
		return NULL;
	}
	
	return ip;
}
