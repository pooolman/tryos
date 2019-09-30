/*
 * 文件系统接口支持函数
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "fs.h"
#include "path.h"
#include "string.h"
#include "inode.h"
#include "file.h"
#include "fcntl.h"


/*
 * 创建普通、目录或者设备文件，如果试图创建普通文件且其已经存在则直接返回其inode引用；未上锁；
 * 成功返回该文件对应inode的引用，失败返回NULL；
 */
mem_inode_t * create(const char * path, uint16_t type, uint16_t major, uint16_t minor)
{
	mem_inode_t * dp;
	mem_inode_t * ip;
	char name[MAX_DIR_NAME_LEN];

	if((dp = resolve_path(path, 1, name)) == NULL)
		return NULL;
	if(strncmp(name, ".", MAX_DIR_NAME_LEN) == 0 || strncmp(name, "..", MAX_DIR_NAME_LEN) == 0)
	{
		release_inode(dp);
		return NULL;
	}
	lock_inode(dp);
	if((ip = lookup_dir(dp, name, NULL)) != NULL)
	{
		unlock_inode(dp);
		release_inode(dp);
		lock_inode(ip);
		if(type == FILE_INODE && ip->type == FILE_INODE)
		{
			/* 已经存在对应的文件，则直接返回对应inode引用 */
			unlock_inode(ip);
			return ip;
		}
		unlock_inode(ip);
		release_inode(ip);
		return NULL;
	}
	
	/* 需要新分配i节点 */
	if((ip = alloc_inode(dp->dev)) == NULL)
		PANIC("create: alloc inode failed");
	lock_inode(ip); //由于该i节点是新分配的，所以同时锁住dp和ip应该不会造成死锁
	ip->type = type;
	ip->major = major;
	ip->minor = minor;
	ip->link_number = 1;
	ip->size = 0;
	memset(ip->addrs, 0, sizeof(ip->addrs));
	
	/* 在父目录中添加目录项，必要时增加其引用计数 */
	add_link(dp, name, ip->inum);
	if(ip->type == DIR_INODE)
	{
		if(add_link(ip, ".", ip->inum) == -1 || add_link(ip, "..", dp->inum) == -1)
			PANIC("create: add ./.. dir entries failed");
		dp->link_number++;
		ip->link_number++;
		update_inode(dp);
	}
	update_inode(ip);

	unlock_inode(dp);
	release_inode(dp);
	unlock_inode(ip);

	return ip;
}

/*
 * 使指定文件与一个新的打开文件结构关联，可能会创建这个文件，返回与这个打开文件结构关联的文件描述符；
 * 成功返回文件描述符，失败返回-1；
 *
 * 注意：
 * 该函数能够打开/创建普通文件（只读、只写、读写）、只读打开目录文件、打开设备文件（只读、只写、读写）；
 */
int32_t do_open(const char * path, uint32_t mode)
{
	mem_inode_t * ip;
	file_t * fp;
	int32_t fd;

	/* 获得ip引用 */
	if(mode & O_CREAT)
	{
		if((ip = create(path, FILE_INODE, 0, 0)) == NULL)
			return -1;
	}
	else
	{
		if((ip = resolve_path(path, 0, NULL)) == NULL)
			return -1;
		lock_inode(ip); //锁住进行进一步检查
		if(ip->type == DIR_INODE)
		{
			if((mode & MODE_RW_MASK) != O_RDONLY)
			{
				unlock_inode(ip);
				release_inode(ip);
				return -1;
			}
		}
		unlock_inode(ip);
	}
	
	/* 分配打开文件结构和关联的文件描述符 */
	if((fp = alloc_file()) == NULL || (fd = alloc_fd(fp)) == -1)
	{
		if(fp)
			close_file(fp);
		release_inode(ip);
		return -1;
	}
	
	/* 设定打开文件结构 */
	fp->type = FD_TYPE_INODE;
	fp->mode = mode & MODE_RW_MASK;
	fp->off = 0;
	fp->ip = ip;
	fp->pipe = NULL;

	return fd;
}

