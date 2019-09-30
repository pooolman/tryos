#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <getopt.h>

#include "./extvars.h"
#include "../tryos/include/fs.h"
#include "./lib.h"
#include "./inode.h"
#include "./path.h"

extern char * optarg;
extern int optind, opterr, optopt;


/* 全局的fd变量/超级块结构体 */
int global_fd; //引用当前正在处理的文件
super_block_t global_sb; //当前文件系统中的超级块

/*
 * 输出i节点元数据（不包括其内容）
 */
static void dump_inode(m_inode_t * ip)
{
	printf("[%u] inode(", (unsigned int)(ip->inum));
	switch(ip->type)
	{
		case DIR_INODE:
			printf("DIR, "); break;
		case CHR_DEV_INODE:
			printf("CHR, "); break;
		case BLK_DEV_INODE:
			printf("BLK, "); break;
		case FILE_INODE:
			printf("FILE, "); break;
		default:
			printf("UNKNOWN-TYPE, "); break;
	}
	printf("<%hu, %hu>, %hu, %u)\n",
			(unsigned short)(ip->major),
			(unsigned short)(ip->minor),
			(unsigned short)(ip->link_number),
			(unsigned int)(ip->size));

}

/*
 * 列出bitmap分配情况，如果是inode bitmap则还输出使用中的i节点元数据，
 * 成功返回空闲bit数，失败打印错误信息并返回0。
 *
 * fd必须引用一个为可读打开的文件，且已经通过make_fs调用格式化过，sb引用该文件系统上的超级块。
 * effect_bits为bitmap中从头开始被统计的bit个数
 * start_sector为该位图起始扇区号，count为该位图所占扇区数
 * is_ibitmap不为0表示目前处理的是inode bitmap，否则表示目前处理的是block bitmap
 */
static uint32_t dump_bitmap(int fd, super_block_t * sb, uint32_t effect_bits, uint32_t start_sector, uint32_t count, int32_t is_ibitmap)
{
	uint32_t free_bits = 0;
	uint8_t mask;
	uint8_t sector[BLOCK_SIZE];
	m_inode_t inode;
	
	inode.fd = fd;
	inode.sb = sb;
	
	for(uint32_t b = 0; b < effect_bits; b += BITS_PER_BLOCK)
	{
		if(raw_read(fd, start_sector + b/BITS_PER_BLOCK, sector, sizeof(sector)) == -1)
		{
			printf("dump_bitmap: read sector error\n");
			return 0;
		}
		for(uint32_t bi = 0; bi < BITS_PER_BLOCK && b+bi < effect_bits; bi++)
		{
			mask = 1 << (bi % 8);
			if((sector[bi/8] & mask) == 0) //空闲的bit
				free_bits++;
			else //在使用中
			{
				if(is_ibitmap) //目前dump的是inode bitmap
				{
					inode.inum = b + bi;
					if(get_inode(&inode) == -1)
					{
						printf("dump_bitmap: get info of inode %u failed\n", (unsigned int)(b + bi));
						return -1;
					}
					dump_inode(&inode);
				}
				else //dump的是block bitmap
					printf("%u ", (unsigned int)(b + bi));
			}
		}
	}

	if( ! is_ibitmap)
		printf("are in use\n");

	return free_bits;
	
}


/*
 * 输出文件系统信息
 * fd必须引用一个为可读可写方式打开的文件，且已经通过make_fs调用格式化过。
 * sb引用该文件系统上的超级块
 * 成功返回0，失败返回-1
 */
int32_t dump_fs(int fd, super_block_t * sb)
{
	
	printf("super block start at %u, occupies %u blocks\n", 1, 1);
	printf("inode bitmap start at %u, occupies %u blocks\n", 2, (unsigned int)sb->blks_ibitmap);
	printf("block bitmap start at %u, occupies %u blocks\n", 2+(unsigned int)(sb->blks_ibitmap), (unsigned int)(sb->blks_bbitmap));
	
	/* inode 相关 */
	printf("inodes erea start at %u, occupies %u blocks, ", 
			2+(unsigned int)(sb->blks_ibitmap + sb->blks_bbitmap), 
			(unsigned int)(sb->blks_inode));
	printf("%u inodes altogether\n", (unsigned int)(sb->inode_number));
	uint32_t avl_inode_num = dump_bitmap(fd, sb, sb->inode_number, 2, sb->blks_ibitmap, 1);
	printf("%u used, remain %u\n", (unsigned int)(sb->inode_number - avl_inode_num), (unsigned int)(avl_inode_num));

	/* block 相关 */
	printf("data blocks erea start at %u, %u blocks altogether\n", 
			2 + (unsigned int)(sb->blks_ibitmap + sb->blks_bbitmap + sb->blks_inode), 
			(unsigned int)(sb->block_number));
	uint32_t avl_block_num = dump_bitmap(fd, sb, sb->block_number, 2 + sb->blks_ibitmap, sb->blks_bbitmap, 0);
	printf("%u used, remain %u\n", (unsigned int)(sb->block_number - avl_block_num), (unsigned int)(avl_block_num));

	return 0;
}

/*
 * 以目录项形式输出path指定目录的目录内容
 * 成功返回0，失败返回-1
 */
int32_t dump_dir(const char * path)
{
	m_inode_t * ip = NULL;
	dirent_t de;
	char dename[MAX_DIR_NAME_LEN + 1];
	
	/* 解析路径 */
	if(resolve_path(path, &ip, 0, NULL) == -1)
	{
		printf("dump_dir: resolve path failed\n");
		return -1;
	}
	if( ! ip)
	{
		printf("dump_dir: path doesn't exist\n");
		return -1;
	}
	
	/* 输出目录项 */
	for(uint32_t off = 0; off < ip->size; off += sizeof(dirent_t))
	{
		if(read_inode(ip, &de, off, sizeof(de)) != sizeof(de))
		{
			printf("dump_dir: read dir failed, maybe this dir is broken\n");
			return -1;
		}
		memcpy(dename, de.name, MAX_DIR_NAME_LEN);
		dename[MAX_DIR_NAME_LEN] = '\0';
		printf("<%s, %u>\n", dename, (unsigned int)(de.inum));
	}

	return 0;
}

/*
 * 以十六进制形式输出path指定文件（普通文件）的内容
 * 成功返回0，失败返回-1
 */
int32_t dump_file(const char * path)
{
	m_inode_t * ip = NULL;
	uint32_t off;
	int32_t size;
	char buf[BLOCK_SIZE];
	
	/* 解析路径 */
	if(resolve_path(path, &ip, 0, NULL) == -1)
	{
		printf("dump_file: resolve path failed\n");
		return -1;
	}
	if( ! ip)
	{
		printf("dump_file: path doesn't exist\n");
		return -1;
	}
	
	/* 输出内容 */
	printf("0x0\t\t\t");
	for(off = 0; off < ip->size; off += (uint32_t)size)
	{
		if((size = read_inode(ip, buf, off, BLOCK_SIZE)) == -1)
		{
			printf("dump_file: read contents of file failed\n");
			return -1;
		}
		for(int32_t i = 0; i < size; i++)
		{
			printf("%hhX ", (unsigned char)buf[i]);
			if(((i + 1) % 32) == 0)
			{
				printf("\n0x%X\t\t\t", (unsigned int)i + (unsigned int)off + 1);
			}
		}
		if(size % 32 != 0)
			printf("\n0x%X", (unsigned int)(ip->size));
		//if(off + (uint32_t)size == ip->size)
		printf("\n");
	}

	return 0;
}



/*
 * 用法：
 * ./cmd fs_img [-d path] [-f path]
 * 
 * 当仅指定fs_img这个参数时，按如下方式输出其超级块信息：
 * 	disk inode bitmap起始位置，所占块数；
 * 	block bitmap起始位置，所占块数；
 * 	disk inode起始位置，所占块数，disk；
 * 	inode总数，已使用个数，剩余个数，列出被使用了的i结点号并打印i结点信息（元数据）； 
 * 	block起始位置，block 总数，已使用个数，剩余个数，列出被使用了的block号； 
 * -d选项：以目录项形式输出path指定目录的目录内容；
 * -f选项：以十六进制形式输出path指定文件的内容；
 */
#define MAX_PATH_LEN 1024

int main(int argc, char * argv[])
{
	int dfnd = 0;
	int ffnd = 0;
	char * dpath = malloc(MAX_PATH_LEN + 1);
	char * fpath = malloc(MAX_PATH_LEN + 1);
	int opt;

	*(dpath + MAX_PATH_LEN) = '\0';
	*(fpath + MAX_PATH_LEN) = '\0';
	
	
	/* 解析参数 */
	while((opt = getopt(argc, argv, "d:f:")) != -1)
	{
		switch(opt)
		{
			case 'd':
				strncpy(dpath, optarg, MAX_PATH_LEN);
				dfnd = 1;
				break;
			case 'f':
				strncpy(fpath, optarg, MAX_PATH_LEN);
				ffnd = 1;
				break;
			default:
				printf("main: %s fs_img [-d path] [-f path]\n", argv[0]);
				return -1;
		}
	}
	if(optind >= argc)
	{
		printf("main: %s fs_img [-d path] [-f path]\n", argv[0]);
		return -1;
	}

	/* 准备fd/sb */
	if((global_fd = open(argv[optind], O_RDONLY)) == -1)
	{
		printf("main: can't open %s\n", argv[optind]);
		return -1;
	}
	if(read_sb(global_fd, &global_sb) == -1)
	{
		printf("main: can't read super block\n");
		return -1;
	}
	
	/* 开始检查 */
	if(dump_fs(global_fd, &global_sb) == -1)
	{
		printf("main: dump fs failed\n");
		return -1;
	}
	if(dfnd && dump_dir(dpath) == -1)
	{
		printf("main: dump dir `%s' failed\n", dpath);
		return -1;
	}

	if(ffnd && dump_file(fpath) == -1)
	{
		printf("main: dump file `%s' failed\n", fpath);
		return -1;
	}

	return 0;
}
