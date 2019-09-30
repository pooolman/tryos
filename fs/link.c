/*
 * 文件系统接口支持函数
 */

#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "fs.h"
#include "path.h"
#include "inode.h"
#include "string.h"

/*
 * 创建一条指向某个文件的路径，成功返回0，失败返回-1；
 * oldpath：指向被链接的文件（不能是目录文件）
 * newpath：是一条新的路径，这条路径将指向oldpath指向的文件
 *
 * 注意：
 * 目前不检测是否跨文件系统创建链接，因为对多文件系统支持不完善
 */
int32_t do_link(const char * oldpath, const char * newpath)
{
	mem_inode_t * oldip;
	mem_inode_t * dp;
	char name[MAX_DIR_NAME_LEN];
	
	/* 进行路径检查 */
	if((oldip = resolve_path(oldpath, 0, NULL)) == NULL)
		return -1;
	if((dp = resolve_path(newpath, 1, name)) == NULL)
	{
		release_inode(oldip);
		return -1;
	}
	
	/* 增加oldpath指向文件的i节点的链接计数 */
	lock_inode(oldip);
	if(oldip->type == DIR_INODE) //不允许链接到目录上
	{
		unlock_inode(oldip);
		release_inode(oldip);
		release_inode(dp);
		return -1;
	}
	if(oldip->link_number > oldip->link_number + 1)
		PANIC("do_link: has reach limits of max link number");
	oldip->link_number++;
	update_inode(oldip);
	unlock_inode(oldip);
	
	/* 新增目录项，如果失败则需要撤回对oldpath指向文件的i节点的修改 */
	lock_inode(dp);
	if(add_link(dp, name, oldip->inum) == -1)
	{
		unlock_inode(dp);
		release_inode(dp);
		
		lock_inode(oldip);
		oldip->link_number--;
		update_inode(oldip);
		unlock_inode(oldip);
		release_inode(oldip);

		return -1;
	}
	unlock_inode(dp);
	release_inode(dp);
	
	return 0;
}

/*
 * 删除指定路径，可能删除这个路径指定的文件，成功返回0，失败返回-1；
 *
 * 注意：
 * 允许调用这个函数删除目录文件；
 * 这个函数能够删除设备文件，但是其含义目前尚未定义；
 * 如果这个path是最后一条指向该文件的路径，且没有任何进程在使用这个文件，那么这个文件将从设备上删除；
 * 如果这个path是最后一条指向该文件的路径，但是还有进程在使用这个文件，那么这个文件将一直存在直到最后一个引用该文件的打开文件被关闭；
 */
int32_t do_unlink(const char * path)
{
	mem_inode_t * dp;
	mem_inode_t * ip;
	char name[MAX_DIR_NAME_LEN];
	uint32_t off;
	dirent_t de;
	
	if((dp = resolve_path(path, 1, name)) == NULL)
		return -1;

	/* 锁住，并检查 */
	lock_inode(dp);
	if(dp->link_number == 0)
		PANIC("do_unlink: maybe the dir is broken ?");
	if(strncmp(name, ".", MAX_DIR_NAME_LEN) == 0 || strncmp(name, "..", MAX_DIR_NAME_LEN) == 0)
	{
		unlock_inode(dp);
		release_inode(dp);
		return -1;
	}
	if((ip = lookup_dir(dp, name, &off)) == NULL)
	{
		unlock_inode(dp);
		release_inode(dp);
		return -1;
	}
	unlock_inode(dp);

	/* 同样锁住，并检查 */
	lock_inode(ip);
	if(ip->link_number == 0)
		PANIC("do_unlink: maybe this file is broken ?");

	if(ip->type == DIR_INODE && !is_empty_dir(ip))
	{
		/* 不能unlink掉非空目录 */
		unlock_inode(ip);
		release_inode(ip);
		release_inode(dp);

		return -1;
	}
	unlock_inode(ip);

	/* 检查完毕进行修改 */
	/* 先修改其父目录 */
	memset(&de, 0, sizeof(de));
	lock_inode(dp);
	if(write_inode(dp, &de, off, sizeof(de)) != sizeof(de))
		PANIC("do_unlink: clear dir entry failed");
	if(ip->type == DIR_INODE)
	{
		dp->link_number--;
		update_inode(dp);
	}
	unlock_inode(dp);
	release_inode(dp);

	/* 再修改这个文件对应inode */
	lock_inode(ip);
	ip->link_number--;
	if(ip->type == DIR_INODE)
		ip->link_number--;
	update_inode(ip);
	unlock_inode(ip);
	release_inode(ip);

	return 0;
}

