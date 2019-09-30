/*
 * 目录/路径层实现
 */

#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "fs.h"
#include "inode.h"
#include "string.h"
#include "process.h"

/*
 * 在指定目录文件中查找具有特定名字的目录项，并返回其对应i结点的inode结构指针，该inode结构未被上锁；
 * 如果找到目录项且off不为NULL，将设置其值为该目录项相对文件开头的偏移量（字节计算）
 * dp: 指定目录，调用该函数前要锁住
 * name: 所查找的目录项名字
 * *poff: 目录项偏移量
 *
 * 注意：
 * 调用者要锁住dp；
 */
mem_inode_t * lookup_dir(mem_inode_t * dp, const char * name, uint32_t * poff)
{
	dirent_t de;
	uint32_t off;

	if( ! (dp->flags & INODE_BUSY) || dp->type != DIR_INODE)
		PANIC("lookup_dir: not a directory or it's unlocked");
	
	for(off = 0; off < dp->size; off += sizeof(dirent_t))
	{
		if(read_inode(dp, &de, off, sizeof(dirent_t)) != sizeof(dirent_t))
			PANIC("lookup_dir: read directory failed, maybe this directory is broken");
		
		if(de.name[0] == '\0') //跳过空的目录项
			continue;
		if(strncmp(de.name, name, MAX_DIR_NAME_LEN) == 0)
		{
			/* 找到目录项 */
			if(poff != NULL)
				*poff = off;
			return acquire_inode(dp->dev, de.inum);
		}
	}
	return NULL;
}


/*
 * 在指定目录文件中添加一个目录项，成功返回0，失败(name已经存在)返回-1。
 * dp: 指定目录
 * name: 待添加的目录项的名字，超出MAX_DIR_NAME_LEN部分被忽略
 * inum: 待添加的目录项指向的i节点编号
 * 注意：
 * 调用者要提前锁住dp；
 * 要保证所提供的i结点编号是有效的，本函数不做检查；
 * 本函数不处理链接计数；
 */
int32_t add_link(mem_inode_t * dp, const char * name, uint32_t inum)
{
	mem_inode_t * ip;
	uint32_t off;
	dirent_t de;

	if( ! (dp->flags & INODE_BUSY) || dp->type != DIR_INODE)
		PANIC("add_link: not a directory or it's unlocked");
	
	if((ip = lookup_dir(dp, name, NULL)) != NULL)
	{
		/* 存在相同目录项 */
		release_inode(ip);
		return -1;
	}
	
	/* 遍历目录文件，查找一个空的目录项 */
	for(off = 0; off < dp->size; off += sizeof(dirent_t))
	{
		if(read_inode(dp, &de, off, sizeof(dirent_t)) != sizeof(dirent_t))
			PANIC("add_link: read directory failed, maybe this directory is broken");
		if(de.name[0] == '\0') //找到一个空的目录项，记录此时的偏移off
			break;
	}
	
	/* 构造新的目录项 */
	de.inum = inum;
	sstrncpy(de.name, name, MAX_DIR_NAME_LEN);

	/* 在off处写入新的目录项，可能会增大文件大小 */
	if(write_inode(dp, &de, off, sizeof(dirent_t)) != sizeof(dirent_t))
		PANIC("add_link: write to directory failed");

	return 0;
}

/*
 * 解析路径中第一个元素并复制到elem中，返回下次解析的位置。
 * 如果未解析出一个元素则返回NULL，否则返回下一次开始解析的位置，如果解析出的元素是最后一个元素则设置last_elem为大于0的值
 * （如果最后一个元素以'/'结尾则为1，否则为2），否则设置为0。
 * path: 待解析的路径
 * elem: 解析出的元素存在该指针指向的位置，必须保证能容纳MAX_DIR_NAME_LEN个字符
 * last_elem: 标志，指示elem是否为path中最后一个元素及其是否以'/'结尾。
 */
static const char * get_element(const char * path, char * elem, int32_t * last_elem)
{
	int32_t i;

	/* 跳过开头的 / 字符 */
	while(*path == '/')
		path++;
	
	/* 复制元素到elem中 */
	for(i = 0; i < MAX_DIR_NAME_LEN && *path != '/' && *path != '\0'; i++)
		*elem++ = *path++;

	if(i == 0) //没有解析出元素
		return NULL;
	else if(i < MAX_DIR_NAME_LEN) //解析出了元素，但是需要在结尾处添加'\0'
		*elem = '\0';

	/* 忽略超出MAX_DIR_NAME_LEN部分的字符 */
	while(*path != '/' && *path != '\0')
		path++;

	if(*path == '\0') //这是最后一个元素了，且不以/结尾
		*last_elem = 2;
	else
	{
		while(*path == '/') //为了检查是否为最后一个元素，继续跳过/字符
			path++;
		if(*path == '\0') //跳过之后到达path结尾，这是最后一个元素，且以/结尾
			*last_elem = 1;
		else
			*last_elem = 0; //跳过之后还有元素
	}
	return path;
}

/*
 * 获取路径中最后一个元素复制到elem中，成功返回大于0的值（以'/'结尾则为1，否则为2）且elem中将包含最后一个元素的值，失败返回-1。
 * path: 指定解析的路径
 * elem: 最后一个元素被写入的位置
 * 注意：
 * elem必须能够容纳最大目录项名字长度；
 */
int32_t get_last_element(const char * path, char * elem)
{
	int32_t last_elem;
	while((path = get_element(path, elem, &last_elem)) != NULL)
	{
		if(last_elem > 0)
			return last_elem;
	}
	return -1;
}


/*
 * 当未指定stop_at_parent时，解析路径，返回该路径指定对象的inode结构指针，未锁住该i结点；
 * 当指定stop_at_parent时，解析路径，返回该路径指定对象的父目录的inode结构指针，未锁住该i节点，同时
 * 如果name不为NULL则还将该路径最后一个元素复制到name中。失败将返回NULL
 * path: 待解析的路径
 * stop_at_parent: 为0时表示解析完整个路径，否则停留在其父目录位置
 * name: 最后一个元素名字，要足够容纳MAX_DIR_NAME_LEN个字符
 *
 * 注意：
 * 针对"/"路径解析并且指定stop_at_parent时，会返回NULL，防止/字符出现在目录项名字中；
 * 如果指定stop_at_parent，则不会检查路径最后一个元素；
 */
mem_inode_t * resolve_path(const char * path, int32_t stop_at_parent, char * name)
{
	int32_t last_elem = 0; //置为0，当解析"/"所指定对象时，由于根目录本身就是一个目录，所以不再进行类型检查。
	mem_inode_t * ip;
	mem_inode_t * next;
	char elem[MAX_DIR_NAME_LEN];


	if(path[0] == '/') //如果是绝对路径从/开始解析
		ip = acquire_inode(ROOT_DEV_NO, ROOT_INUM);
	else //否则从进程当前工作目录开始解析
		ip = dup_inode(cpu.cur_proc->cwd);
	
	while((path = get_element(path, elem, &last_elem)) != NULL)
	{
		lock_inode(ip); //读取并锁住这个i节点

		if(ip->type != DIR_INODE) //这个i节点必须是包含上述元素elem的父目录
		{
			unlock_inode(ip);
			release_inode(ip);
			return NULL;
		}
		
		if(stop_at_parent && last_elem > 0) //在最后一个元素的父目录位置停止解析
		{
			unlock_inode(ip);
			if(name)
				sstrncpy(name, elem, MAX_DIR_NAME_LEN);
			return ip; //并返回其父目录的inode结构指针
		}
		
		if((next = lookup_dir(ip, elem, NULL)) == NULL) //不存在这个元素
		{
			unlock_inode(ip);
			release_inode(ip);
			return NULL;
		}
		unlock_inode(ip);
		release_inode(ip);
		ip = next;
	}

	if(stop_at_parent) //说明此时解析"/"的父目录
	{
		/* 所以直接返回NULL */
		release_inode(ip);
		return NULL;
	}
	if(last_elem == 1) //最后一个元素以'/'结尾，必须进一步检查是否是一个目录
	{
		lock_inode(ip);
		if(ip->type != DIR_INODE)
		{
			unlock_inode(ip);
			release_inode(ip);
			return NULL;
		}
		unlock_inode(ip);
	}
	return ip;
}

/*
 * 检查指定目录是否为空目录（仅包含./..两个目录项），为空则返回1，否则返回0。
 * 注意：
 * 调用者需要锁住dp；
 */
int32_t is_empty_dir(mem_inode_t * dp)
{
	dirent_t de;
	uint32_t off;

	for(off = 2 * sizeof(dirent_t); off < dp->size; off += sizeof(dirent_t))
	{
		if(read_inode(dp, &de, off, sizeof(dirent_t)) != sizeof(dirent_t))
			PANIC("is_empty_dir: maybe this directory is broken");
		if(de.name[0] != '\0')
			return 0;
	}
	return 1;
}
