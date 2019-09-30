#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "../tryos/include/fs.h"
#include "./inode.h"

/*
 * 在指定目录文件中查询目录项，成功查询返回0，如果存在该名字的目录项则将其对应i结点的内存i结点指针写入ipp，否则返回-1，不设置ipp
 * 注意：
 * 该函数不检查dp是否引用一个目录文件；
 * 所返回的内存i节点结构中没有有效的数据，需要读取一次；
 */
int32_t lookup_dir(m_inode_t * dp, const char * name, m_inode_t ** ipp)
{
	dirent_t de;

	for(uint32_t off = 0; off < dp->size; off += sizeof(dirent_t))
	{
		if(read_inode(dp, &de, off, sizeof(de)) != sizeof(de))
		{
			printf("lookup_dir: read dir failed, maybe this dir is broken\n");
			return -1;
		}
		if(de.name[0] == '\0')
			continue;

		if(strncmp(de.name, name, MAX_DIR_NAME_LEN) == 0)
		{
			*ipp = acquire_inode(de.inum);
			break;
		}
	}
	return 0;
}


/*
 * 在指定目录文件中添加目录项，成功返回0，失败返回-1
 */
int32_t add_link(m_inode_t * dp, const char * name, uint32_t inum)
{
	dirent_t de;

	if(dp->type != DIR_INODE)
	{
		printf("add_link: not a directory\n");
		return -1;
	}
	
	/* 保证不存在同名目录项 */
	m_inode_t * ip = NULL;
	if(lookup_dir(dp, name, &ip) == -1)
	{
		printf("add_link: lookup dir failed\n");
		return -1;
	}
	if(ip)
	{
		printf("add_link: this item already exist in directory\n");
		return -1;
	}

	/* 找空的目录项 */
	uint32_t off;
	for(off = 0; off < dp->size; off += sizeof(dirent_t))
	{
		if(read_inode(dp, &de, off, sizeof(de)) != sizeof(de))
		{
			printf("add_link: read dir failed, maybe this dir is broken\n");
			return -1;
		}
		
		if(de.name[0] == '\0')
			break;
	}

	/* 构造待写入的目录项 */
	de.inum = inum;
	strncpy(de.name, name, MAX_DIR_NAME_LEN);

	/* 写入目录 */
	if(write_inode(dp, &de, off, sizeof(de)) != sizeof(de))
	{
		printf("add_link: write dir failed\n");
		return -1;
	}
	
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
char * get_element(const char * path, char * elem, int32_t * last_elem)
{
	int32_t i;

	/*
	// DEBUG
	char * elem_copy = elem;
	printf("get_element: ready to get element from |%s|\n", path);
	*/

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

	/*
	// DEBUG
	printf("get_element: get element: %d bytes: ", (int)i);
	for(int32_t j = 0; j < MAX_DIR_NAME_LEN; j++)
		printf("%c ", elem_copy[j]);
	printf("\n");
	*/

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
	return (char *)path;
}


/*
 * 解析路径，操作成功返回0，解析成功将该路径指定对象的内存i结点结构指针或者其父目录对应的内存i结点结构指针写入ipp中，如果是针对其父目录，并且name不为NULL的话则还将该路径指定对象的名字复制到name中。操作失败则返回-1，不修改name/ipp的值。
 *
 * 注意：
 * 对"/"路径查找其父目录，会返回NULL，以防止'/'出现在目录项名中；
 * 如果仅查找路径指定对象的父目录，则不检查路径指定对象是否存在；
 * 返回的内存i结点结构中存在有效数据；
 * 所有path均视为绝对路径；
 */
int32_t resolve_path(const char * path, m_inode_t ** ipp, int32_t stop_at_parent, char * name)
{
	m_inode_t * ip;
	m_inode_t * next;

	char elem[MAX_DIR_NAME_LEN];
	int32_t last_elem = 0;
	

	/* 获取根目录i节点 */
	if((ip = acquire_inode(ROOT_INUM)) == NULL)
	{
		printf("resolve_path: acquire root inode failed\n");
		return -1;
	}
	
	/* 开始解析 */
	while((path = get_element(path, elem, &last_elem)) != NULL)
	{
		if(get_inode(ip) == -1)
		{
			printf("resolve_path: read inode info failed\n");
			return -1;
		}
		if(ip->type != DIR_INODE)
		{
			free(ip);
			return 0;
		}
		
		if(stop_at_parent && last_elem > 0)
		{
			if(name)
				strncpy(name, elem, MAX_DIR_NAME_LEN);
			*ipp = ip;
			return 0;
		}
		
		/* 在目录中查找这个元素 */
		next = NULL;
		if(lookup_dir(ip, elem, &next) == -1)
		{
			printf("resolve_path: lookup dir failed\n");
			free(ip);
			return -1;
		}

		free(ip);
		if( ! next)
			return 0;
		ip = next;
	}
	
	if(stop_at_parent) //此时是针对"/"解析其parent dir
	{
		free(ip);
		return 0;
	}
	
	if(get_inode(ip) == -1)
	{
		printf("resolve_path: read target inode failed\n");
		free(ip);
		return -1;
	}

	if(last_elem == 1 && ip->type != DIR_INODE) //path指定对象应该是目录
	{
		free(ip);
		return 0;
	}
	
	*ipp = ip;
	return 0;

}
